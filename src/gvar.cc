// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gvar.h"

#include "fvar.h"
#include "maxp.h"

#define TABLE_NAME "gvar"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeGVAR
// -----------------------------------------------------------------------------

static bool ParseSharedTuples(const Font* font, const uint8_t* data, size_t length,
                              size_t sharedTupleCount, size_t axisCount) {
  Buffer subtable(data, length);
  for (unsigned i = 0; i < sharedTupleCount; i++) {
    for (unsigned j = 0; j < axisCount; j++) {
      int16_t coordinate;
      if (!subtable.ReadS16(&coordinate)) {
        return OTS_FAILURE_MSG("Failed to read shared tuple coordinate");
      }
    }
  }
  return true;
}

static bool ParseGlyphVariationData(const Font* font, const uint8_t* data, size_t length,
                                    size_t axisCount, size_t sharedTupleCount) {
  Buffer subtable(data, length);

  uint16_t tupleVariationCount;
  uint16_t dataOffset;
  if (!subtable.ReadU16(&tupleVariationCount) ||
      !subtable.ReadU16(&dataOffset)) {
    return OTS_FAILURE_MSG("Failed to read glyph variation data header");
  }

  if (dataOffset > length) {
    return OTS_FAILURE_MSG("Invalid serialized data offset");
  }

  tupleVariationCount &= 0x0FFF; // mask off flags

  const uint16_t EMBEDDED_PEAK_TUPLE = 0x8000;
  const uint16_t INTERMEDIATE_REGION = 0x4000;
  const uint16_t TUPLE_INDEX_MASK    = 0x0FFF;

  for (unsigned i = 0; i < tupleVariationCount; i++) {
    uint16_t variationDataSize;
    uint16_t tupleIndex;

    if (!subtable.ReadU16(&variationDataSize) ||
        !subtable.ReadU16(&tupleIndex)) {
      return OTS_FAILURE_MSG("Failed to read tuple variation header");
    }

    if (tupleIndex & EMBEDDED_PEAK_TUPLE) {
      for (unsigned axis = 0; axis < axisCount; axis++) {
        int16_t coordinate;
        if (!subtable.ReadS16(&coordinate)) {
          return OTS_FAILURE_MSG("Failed to read tuple coordinate");
        }
        if (coordinate < -0x4000 || coordinate > 0x4000) {
          return OTS_FAILURE_MSG("Invalid tuple coordinate");
        }
      }
    }

    if (tupleIndex & INTERMEDIATE_REGION) {
      std::vector<int16_t> startTuple(axisCount);
      for (unsigned axis = 0; axis < axisCount; axis++) {
        int16_t coordinate;
        if (!subtable.ReadS16(&coordinate)) {
          return OTS_FAILURE_MSG("Failed to read tuple coordinate");
        }
        if (coordinate < -0x4000 || coordinate > 0x4000) {
          return OTS_FAILURE_MSG("Invalid tuple coordinate");
        }
        startTuple.push_back(coordinate);
      }

      std::vector<int16_t> endTuple(axisCount);
      for (unsigned axis = 0; axis < axisCount; axis++) {
        int16_t coordinate;
        if (!subtable.ReadS16(&coordinate)) {
          return OTS_FAILURE_MSG("Failed to read tuple coordinate");
        }
        if (coordinate < -0x4000 || coordinate > 0x4000) {
          return OTS_FAILURE_MSG("Invalid tuple coordinate");
        }
        endTuple.push_back(coordinate);
      }

      for (unsigned axis = 0; axis < axisCount; axis++) {
        if (startTuple[axis] > endTuple[axis]) {
          return OTS_FAILURE_MSG("Invalid intermediate range");
        }
      }
    }

    tupleIndex &= TUPLE_INDEX_MASK;
    if (tupleIndex >= sharedTupleCount) {
      return OTS_FAILURE_MSG("Tuple index out of range");
    }
  }

  // TODO: we don't attempt to interpret the serialized data block

  return true;
}

static bool ParseGlyphVariationDataArray(const Font* font, const uint8_t* data, size_t length,
                                         uint16_t flags, size_t glyphCount, size_t axisCount,
                                         size_t sharedTupleCount,
                                         const uint8_t* glyphVariationData,
                                         size_t glyphVariationDataLength) {
  Buffer subtable(data, length);

  bool glyphVariationDataOffsetsAreLong = (flags & 0x0001u);
  uint32_t prevOffset = 0;
  for (size_t i = 0; i < glyphCount + 1; i++) {
    uint32_t offset;
    if (glyphVariationDataOffsetsAreLong) {
      if (!subtable.ReadU32(&offset)) {
        return OTS_FAILURE_MSG("Failed to read GlyphVariationData offset");
      }
    } else {
      uint16_t halfOffset;
      if (!subtable.ReadU16(&halfOffset)) {
        return OTS_FAILURE_MSG("Failed to read GlyphVariationData offset");
      }
      offset = halfOffset * 2;
    }

    if (i > 0 && offset > prevOffset) {
      if (prevOffset > glyphVariationDataLength) {
        return OTS_FAILURE_MSG("Invalid GlyphVariationData offset");
      }
      if (!ParseGlyphVariationData(font, glyphVariationData + prevOffset,
                                   glyphVariationDataLength - prevOffset,
                                   axisCount, sharedTupleCount)) {
        return OTS_FAILURE_MSG("Failed to parse GlyphVariationData");
      }
    }
    prevOffset = offset;
  }

  return true;
}

bool OpenTypeGVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);

  uint16_t majorVersion;
  uint16_t minorVersion;
  uint16_t axisCount;
  uint16_t sharedTupleCount;
  uint32_t sharedTuplesOffset;
  uint16_t glyphCount;
  uint16_t flags;
  uint32_t glyphVariationDataArrayOffset;

  if (!table.ReadU16(&majorVersion) ||
      !table.ReadU16(&minorVersion) ||
      !table.ReadU16(&axisCount) ||
      !table.ReadU16(&sharedTupleCount) ||
      !table.ReadU32(&sharedTuplesOffset) ||
      !table.ReadU16(&glyphCount) ||
      !table.ReadU16(&flags) ||
      !table.ReadU32(&glyphVariationDataArrayOffset)) {
    return DropVariations("Failed to read table header");
  }
  if (majorVersion != 1) {
    return DropVariations("Unknown table version");
  }

  // check axisCount == fvar->axisCount
  OpenTypeFVAR* fvar = static_cast<OpenTypeFVAR*>(
      GetFont()->GetTypedTable(OTS_TAG_FVAR));
  if (!fvar) {
    return DropVariations("Required fvar table is missing");
  }
  if (axisCount != fvar->AxisCount()) {
    return DropVariations("Axis count mismatch");
  }

  // check glyphCount == maxp->num_glyphs
  OpenTypeMAXP* maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return DropVariations("Required maxp table is missing");
  }
  if (glyphCount != maxp->num_glyphs) {
    return DropVariations("Glyph count mismatch");
  }

  if (sharedTupleCount > 0) {
    if (sharedTuplesOffset < table.offset() || sharedTuplesOffset > length) {
      return DropVariations("Invalid sharedTuplesOffset");
    }
    if (!ParseSharedTuples(GetFont(),
                           data + sharedTuplesOffset, length - sharedTuplesOffset,
                           sharedTupleCount, axisCount)) {
      return DropVariations("Failed to parse shared tuples");
    }
  }

  if (glyphVariationDataArrayOffset) {
    if (glyphVariationDataArrayOffset > length) {
      return DropVariations("Invalid glyphVariationDataArrayOffset");
    }
    if (!ParseGlyphVariationDataArray(GetFont(),
                                      data + table.offset(), length - table.offset(),
                                      flags, glyphCount, axisCount, sharedTupleCount,
                                      data + glyphVariationDataArrayOffset,
                                      length - glyphVariationDataArrayOffset)) {
      return DropVariations("Failed to read glyph variation data array");
    }
  }

  this->m_data = data;
  this->m_length = length;

  return true;
}

bool OpenTypeGVAR::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write gvar table");
  }

  return true;
}

}  // namespace ots
