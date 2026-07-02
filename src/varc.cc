// Copyright (c) 2025 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "varc.h"

#include "fvar.h"
#include "layout.h"
#include "maxp.h"

#include <vector>

// VARC - Variable Composites / Components Table
// https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md

#define TABLE_NAME "VARC"

namespace {

// Variable Component flags.
// https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#variable-component-flags
enum VarcFlags : uint32_t {
  RESET_UNSPECIFIED_AXES     = 1u << 0,
  HAVE_AXES                  = 1u << 1,
  AXIS_VALUES_HAVE_VARIATION = 1u << 2,
  TRANSFORM_HAS_VARIATION    = 1u << 3,
  HAVE_TRANSLATE_X           = 1u << 4,
  HAVE_TRANSLATE_Y           = 1u << 5,
  HAVE_ROTATION              = 1u << 6,
  HAVE_CONDITION             = 1u << 7,
  HAVE_SCALE_X               = 1u << 8,
  HAVE_SCALE_Y               = 1u << 9,
  HAVE_TCENTER_X             = 1u << 10,
  HAVE_TCENTER_Y             = 1u << 11,
  GID_IS_24BIT               = 1u << 12,
  HAVE_SKEW_X                = 1u << 13,
  HAVE_SKEW_Y                = 1u << 14,
  RESERVED_MASK              = ~((1u << 15) - 1),
};

// State collected while parsing, used to validate cross-references between the
// various sub-tables.
struct varcState {
  uint16_t numGlyphs = 0;      // from maxp
  uint16_t axisCount = 0;      // from fvar (0 if no fvar table)

  // MultiItemVariationStore: number of delta sets ("inner" index range) for
  // each item variation data sub-table ("outer" index). Empty if no varStore.
  std::vector<uint32_t> deltaSetCounts;

  // Number of entries in the top-level ConditionList (0 if absent).
  uint32_t conditionCount = 0;

  // Number of axis-value entries encoded by each entry of the axisIndicesList
  // (i.e. the number of axes each glyph component referring to it must supply
  // axis values for). Empty if the axisIndicesList is absent.
  std::vector<uint32_t> axisIndicesCounts;
};

// A single object within a CFF2-style Index, expressed as an offset and length
// relative to the start of the Index structure.
struct IndexObject {
  uint32_t offset;
  uint32_t length;
};

// Read a uint32var: a variable-length (1-5 byte) encoding of a uint32.
// https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#uint32var
bool ReadUint32Var(ots::Buffer& buf, uint32_t* value) {
  uint8_t b0;
  if (!buf.ReadU8(&b0)) {
    return false;
  }
  if (b0 < 0x80) {
    *value = b0;
    return true;
  }
  if (b0 < 0xC0) {
    uint8_t b1;
    if (!buf.ReadU8(&b1)) {
      return false;
    }
    *value = (static_cast<uint32_t>(b0 - 0x80) << 8) | b1;
    return true;
  }
  if (b0 < 0xE0) {
    uint8_t b1, b2;
    if (!buf.ReadU8(&b1) || !buf.ReadU8(&b2)) {
      return false;
    }
    *value = (static_cast<uint32_t>(b0 - 0xC0) << 16) |
             (static_cast<uint32_t>(b1) << 8) | b2;
    return true;
  }
  if (b0 < 0xF0) {
    uint8_t b1, b2, b3;
    if (!buf.ReadU8(&b1) || !buf.ReadU8(&b2) || !buf.ReadU8(&b3)) {
      return false;
    }
    *value = (static_cast<uint32_t>(b0 - 0xE0) << 24) |
             (static_cast<uint32_t>(b1) << 16) |
             (static_cast<uint32_t>(b2) << 8) | b3;
    return true;
  }
  // 5-byte form. The high nibble (b0 - 0xF0) contributes bits at or above 32,
  // which do not fit in a uint32; valid encodings use a lead byte of exactly
  // 0xF0, so those bits are zero. We keep only the low 32 bits.
  uint8_t b1, b2, b3, b4;
  if (!buf.ReadU8(&b1) || !buf.ReadU8(&b2) || !buf.ReadU8(&b3) ||
      !buf.ReadU8(&b4)) {
    return false;
  }
  *value = (static_cast<uint32_t>(b1) << 24) |
           (static_cast<uint32_t>(b2) << 16) |
           (static_cast<uint32_t>(b3) << 8) | b4;
  return true;
}

// Consume a TupleValues (packed deltas) structure, advancing |buf|.
//
// TupleValues is the TupleVariationStore packed-deltas encoding extended so
// that a control byte with both the ZEROS and WORDS bits set introduces 32-bit
// values.
//
// If |count_known| is true, exactly |known_count| values are decoded (and the
// buffer is left positioned immediately after them). Otherwise values are
// decoded until |buf| is exhausted, and *out_count receives the number decoded.
bool ParseTupleValues(const ots::Font* font, ots::Buffer& buf, bool count_known,
                      size_t known_count, size_t* out_count) {
  static const uint8_t VALUES_SIZE_MASK = 0xC0;
  static const uint8_t VALUES_ARE_BYTES = 0x00;
  static const uint8_t VALUES_ARE_WORDS = 0x40;
  static const uint8_t VALUES_ARE_ZEROS = 0x80;
  static const uint8_t VALUES_ARE_LONGS = 0xC0;
  static const uint8_t RUN_COUNT_MASK = 0x3F;

  size_t total = 0;
  for (;;) {
    if (count_known) {
      if (total >= known_count) {
        break;
      }
    } else if (buf.remaining() == 0) {
      break;
    }

    uint8_t control;
    if (!buf.ReadU8(&control)) {
      return OTS_FAILURE_MSG("Failed to read TupleValues control byte");
    }

    size_t run = static_cast<size_t>(control & RUN_COUNT_MASK) + 1;
    if (count_known && total + run > known_count) {
      return OTS_FAILURE_MSG("TupleValues run overshoots expected count");
    }

    size_t elemSize;
    switch (control & VALUES_SIZE_MASK) {
      case VALUES_ARE_ZEROS: elemSize = 0; break;
      case VALUES_ARE_BYTES: elemSize = 1; break;
      case VALUES_ARE_WORDS: elemSize = 2; break;
      case VALUES_ARE_LONGS: elemSize = 4; break;
      default: elemSize = 0; break;  // unreachable
    }

    if (elemSize && !buf.Skip(run * elemSize)) {
      return OTS_FAILURE_MSG("Failed to read TupleValues data");
    }

    total += run;
  }

  if (out_count) {
    *out_count = total;
  }
  return true;
}

// Parse and validate a CFF2-style Index (a count of uint32, then an offset
// array, then object data). The objects are returned (if |objects| is non-NULL)
// as offsets/lengths relative to |data|; *out_count receives the object count.
// https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#cff2indexof
bool ParseCFF2Index(const ots::Font* font, const uint8_t* data, size_t length,
                    uint32_t* out_count, std::vector<IndexObject>* objects) {
  ots::Buffer table(data, length);

  uint32_t count;
  if (!table.ReadU32(&count)) {
    return OTS_FAILURE_MSG("Failed to read Index count");
  }

  if (out_count) {
    *out_count = count;
  }
  if (objects) {
    objects->clear();
  }

  if (count == 0) {
    return true;
  }

  uint8_t offSize;
  if (!table.ReadU8(&offSize)) {
    return OTS_FAILURE_MSG("Failed to read Index offSize");
  }
  if (offSize < 1 || offSize > 4) {
    return OTS_FAILURE_MSG("Bad Index offSize: %u", offSize);
  }

  // The offset array holds count + 1 entries of offSize bytes each, followed by
  // the object data. Offsets are 1-based, relative to the byte preceding the
  // object data.
  const uint64_t arraySize = (static_cast<uint64_t>(count) + 1) * offSize;
  const uint64_t objectDataOffset = static_cast<uint64_t>(table.offset()) + arraySize;
  if (objectDataOffset > length) {
    return OTS_FAILURE_MSG("Index offset array out of bounds");
  }

  uint64_t prevAbs = 0;
  for (uint32_t i = 0; i <= count; ++i) {
    uint32_t rel = 0;
    for (unsigned b = 0; b < offSize; ++b) {
      uint8_t byte;
      if (!table.ReadU8(&byte)) {
        return OTS_FAILURE_MSG("Failed to read Index offset");
      }
      rel = (rel << 8) | byte;
    }

    if (rel < 1) {
      return OTS_FAILURE_MSG("Bad Index offset (zero)");
    }
    if (i == 0 && rel != 1) {
      return OTS_FAILURE_MSG("First Index offset must be 1");
    }

    const uint64_t abs = objectDataOffset + (static_cast<uint64_t>(rel) - 1);
    if (abs > length) {
      return OTS_FAILURE_MSG("Index object out of bounds");
    }
    if (i > 0) {
      if (abs < prevAbs) {
        return OTS_FAILURE_MSG("Index offsets out of order");
      }
      if (objects) {
        IndexObject obj;
        obj.offset = static_cast<uint32_t>(prevAbs);
        obj.length = static_cast<uint32_t>(abs - prevAbs);
        objects->push_back(obj);
      }
    }
    prevAbs = abs;
  }

  return true;
}

// One entry of a SparseVariationRegion.
bool ParseSparseVariationRegion(const ots::Font* font, const uint8_t* data,
                                size_t length, const varcState& state) {
  ots::Buffer subtable(data, length);

  uint16_t regionAxisCount;
  if (!subtable.ReadU16(&regionAxisCount)) {
    return OTS_FAILURE_MSG("Failed to read sparse region axis count");
  }

  for (unsigned i = 0; i < regionAxisCount; ++i) {
    uint16_t axisIndex;
    int16_t startCoord, peakCoord, endCoord;
    if (!subtable.ReadU16(&axisIndex) ||
        !subtable.ReadS16(&startCoord) ||
        !subtable.ReadS16(&peakCoord) ||
        !subtable.ReadS16(&endCoord)) {
      return OTS_FAILURE_MSG("Failed to read sparse region axis coordinates");
    }

    if (axisIndex >= state.axisCount) {
      return OTS_FAILURE_MSG("Sparse region axis index %u out of range", axisIndex);
    }
    if (startCoord > peakCoord || peakCoord > endCoord) {
      return OTS_FAILURE_MSG("Region axis coordinates out of order");
    }
    if (startCoord < -0x4000 || endCoord > 0x4000) {
      return OTS_FAILURE_MSG("Region axis coordinate out of range");
    }
    if ((peakCoord < 0 && endCoord > 0) ||
        (peakCoord > 0 && startCoord < 0)) {
      return OTS_FAILURE_MSG("Invalid region axis coordinates");
    }
  }

  return true;
}

bool ParseSparseVariationRegionList(const ots::Font* font, const uint8_t* data,
                                    size_t length, const varcState& state,
                                    uint16_t* out_regionCount) {
  ots::Buffer subtable(data, length);

  uint16_t regionCount;
  if (!subtable.ReadU16(&regionCount)) {
    return OTS_FAILURE_MSG("Failed to read sparse region list count");
  }
  *out_regionCount = regionCount;

  for (unsigned i = 0; i < regionCount; ++i) {
    uint32_t offset;
    if (!subtable.ReadU32(&offset)) {
      return OTS_FAILURE_MSG("Failed to read sparse region offset");
    }
    if (offset < 2u || offset >= length) {
      return OTS_FAILURE_MSG("Bad sparse region offset");
    }
    if (!ParseSparseVariationRegion(font, data + offset, length - offset, state)) {
      return OTS_FAILURE_MSG("Failed to parse sparse variation region %u", i);
    }
  }

  return true;
}

// A single MultiItemVariationData sub-table. *out_deltaSetCount receives the
// number of delta sets it stores (the "inner" index range).
bool ParseMultiItemVariationData(const ots::Font* font, const uint8_t* data,
                                 size_t length, uint16_t regionCount,
                                 uint32_t* out_deltaSetCount) {
  ots::Buffer subtable(data, length);

  uint8_t format;
  uint16_t regionIndexCount;
  if (!subtable.ReadU8(&format) || !subtable.ReadU16(&regionIndexCount)) {
    return OTS_FAILURE_MSG("Failed to read MultiItemVariationData header");
  }
  if (format != 1) {
    return OTS_FAILURE_MSG("Unknown MultiItemVariationData format: %u", format);
  }

  for (unsigned i = 0; i < regionIndexCount; ++i) {
    uint16_t regionIndex;
    if (!subtable.ReadU16(&regionIndex)) {
      return OTS_FAILURE_MSG("Failed to read region index");
    }
    if (regionIndex >= regionCount) {
      return OTS_FAILURE_MSG("Region index %u out of range", regionIndex);
    }
  }

  // The remaining bytes are a CFF2-style Index of TupleValues (the delta sets).
  const size_t indexStart = subtable.offset();
  if (!ParseCFF2Index(font, data + indexStart, length - indexStart,
                      out_deltaSetCount, NULL)) {
    return OTS_FAILURE_MSG("Failed to parse delta sets Index");
  }

  return true;
}

// MultiItemVariationStore. Fills state.deltaSetCounts.
bool ParseMultiItemVariationStore(const ots::Font* font, const uint8_t* data,
                                  size_t length, varcState* state) {
  ots::Buffer subtable(data, length);

  uint16_t format;
  uint32_t regionListOffset;
  uint16_t dataCount;
  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU32(&regionListOffset) ||
      !subtable.ReadU16(&dataCount)) {
    return OTS_FAILURE_MSG("Failed to read MultiItemVariationStore header");
  }
  if (format != 1) {
    return OTS_FAILURE_MSG("Unknown MultiItemVariationStore format: %u", format);
  }

  const size_t headerEnd = subtable.offset() + static_cast<size_t>(dataCount) * 4;
  if (regionListOffset < headerEnd || regionListOffset >= length) {
    return OTS_FAILURE_MSG("Bad region list offset");
  }

  uint16_t regionCount = 0;
  if (!ParseSparseVariationRegionList(font, data + regionListOffset,
                                      length - regionListOffset, *state,
                                      &regionCount)) {
    return OTS_FAILURE_MSG("Failed to parse sparse variation region list");
  }

  state->deltaSetCounts.clear();
  for (unsigned i = 0; i < dataCount; ++i) {
    uint32_t offset;
    if (!subtable.ReadU32(&offset)) {
      return OTS_FAILURE_MSG("Failed to read item variation data offset");
    }
    if (offset < headerEnd || offset >= length) {
      return OTS_FAILURE_MSG("Bad item variation data offset");
    }
    uint32_t deltaSetCount = 0;
    if (!ParseMultiItemVariationData(font, data + offset, length - offset,
                                     regionCount, &deltaSetCount)) {
      return OTS_FAILURE_MSG("Failed to parse item variation data %u", i);
    }
    state->deltaSetCounts.push_back(deltaSetCount);
  }

  return true;
}

// Validate a VarIdx (outer index in the top 16 bits, inner in the low 16 bits)
// against the MultiItemVariationStore.
bool ValidateVarIdx(const ots::Font* font, uint32_t varIdx, const varcState& state) {
  const uint16_t outer = varIdx >> 16;
  const uint16_t inner = varIdx & 0xFFFFu;
  if (outer >= state.deltaSetCounts.size()) {
    return OTS_FAILURE_MSG("VarIdx outer index %u out of range", outer);
  }
  if (inner >= state.deltaSetCounts[outer]) {
    return OTS_FAILURE_MSG("VarIdx inner index %u out of range", inner);
  }
  return true;
}

// Conditions may nest (formats 3, 4 and 5); bound the recursion.
const uint32_t kConditionRecursionLimit = 64;

bool ParseCondition(const ots::Font* font, const uint8_t* data, size_t length,
                    const varcState& state, uint32_t depth) {
  if (depth > kConditionRecursionLimit) {
    return OTS_FAILURE_MSG("Excessive condition nesting");
  }

  ots::Buffer subtable(data, length);

  uint16_t format;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE_MSG("Failed to read condition format");
  }

  switch (format) {
    case 1: {  // ConditionAxisRange
      uint16_t axisIndex;
      int16_t filterRangeMin, filterRangeMax;
      if (!subtable.ReadU16(&axisIndex) ||
          !subtable.ReadS16(&filterRangeMin) ||
          !subtable.ReadS16(&filterRangeMax)) {
        return OTS_FAILURE_MSG("Failed to read condition format 1");
      }
      if (axisIndex >= state.axisCount) {
        return OTS_FAILURE_MSG("Condition axis index %u out of range", axisIndex);
      }
      return true;
    }

    case 2: {  // ConditionValue (uses the variation store)
      int16_t defaultValue;
      uint32_t varIdx;
      if (!subtable.ReadS16(&defaultValue) || !subtable.ReadU32(&varIdx)) {
        return OTS_FAILURE_MSG("Failed to read condition format 2");
      }
      if (!ValidateVarIdx(font, varIdx, state)) {
        return OTS_FAILURE_MSG("Bad VarIdx in condition format 2");
      }
      return true;
    }

    case 3:    // ConditionAnd
    case 4: {  // ConditionOr
      uint8_t conditionCount;
      if (!subtable.ReadU8(&conditionCount)) {
        return OTS_FAILURE_MSG("Failed to read condition format %u count", format);
      }
      for (unsigned i = 0; i < conditionCount; ++i) {
        uint32_t offset;
        if (!subtable.ReadU24(&offset)) {
          return OTS_FAILURE_MSG("Failed to read child condition offset");
        }
        if (offset < 2u || offset >= length) {
          return OTS_FAILURE_MSG("Bad child condition offset");
        }
        if (!ParseCondition(font, data + offset, length - offset, state,
                            depth + 1)) {
          return OTS_FAILURE_MSG("Failed to parse child condition");
        }
      }
      return true;
    }

    case 5: {  // ConditionNegate
      uint32_t offset;
      if (!subtable.ReadU24(&offset)) {
        return OTS_FAILURE_MSG("Failed to read negated condition offset");
      }
      if (offset < 2u || offset >= length) {
        return OTS_FAILURE_MSG("Bad negated condition offset");
      }
      if (!ParseCondition(font, data + offset, length - offset, state,
                          depth + 1)) {
        return OTS_FAILURE_MSG("Failed to parse negated condition");
      }
      return true;
    }

    default:
      return OTS_FAILURE_MSG("Unknown condition format: %u", format);
  }
}

bool ParseConditionList(const ots::Font* font, const uint8_t* data,
                        size_t length, varcState* state) {
  ots::Buffer subtable(data, length);

  uint32_t conditionCount;
  if (!subtable.ReadU32(&conditionCount)) {
    return OTS_FAILURE_MSG("Failed to read condition list count");
  }
  state->conditionCount = conditionCount;

  for (unsigned i = 0; i < conditionCount; ++i) {
    uint32_t offset;
    if (!subtable.ReadU32(&offset)) {
      return OTS_FAILURE_MSG("Failed to read condition offset");
    }
    if (offset < 4u || offset >= length) {
      return OTS_FAILURE_MSG("Bad condition offset");
    }
    if (!ParseCondition(font, data + offset, length - offset, *state, 0)) {
      return OTS_FAILURE_MSG("Failed to parse condition %u", i);
    }
  }

  return true;
}

// axisIndicesList: a CFF2-style Index whose entries are TupleValues encoding the
// axis indices used by glyph components. Fills state.axisIndicesCounts with the
// number of values in each entry.
bool ParseAxisIndicesList(const ots::Font* font, const uint8_t* data,
                          size_t length, varcState* state) {
  uint32_t count = 0;
  std::vector<IndexObject> objects;
  if (!ParseCFF2Index(font, data, length, &count, &objects)) {
    return OTS_FAILURE_MSG("Failed to parse axisIndicesList Index");
  }

  state->axisIndicesCounts.clear();
  for (const auto& obj : objects) {
    ots::Buffer entry(data + obj.offset, obj.length);
    size_t numValues = 0;
    if (!ParseTupleValues(font, entry, /*count_known=*/false, 0, &numValues)) {
      return OTS_FAILURE_MSG("Failed to parse axisIndices entry");
    }
    state->axisIndicesCounts.push_back(static_cast<uint32_t>(numValues));
  }

  return true;
}

// A single Variable Component record, advancing |rec|.
// https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#variable-component-record
bool ParseVarComponent(const ots::Font* font, ots::Buffer& rec,
                       const varcState& state) {
  uint32_t flags;
  if (!ReadUint32Var(rec, &flags)) {
    return OTS_FAILURE_MSG("Failed to read component flags");
  }

  // gid
  uint32_t gid;
  if (flags & GID_IS_24BIT) {
    if (!rec.ReadU24(&gid)) {
      return OTS_FAILURE_MSG("Failed to read component gid (24-bit)");
    }
  } else {
    uint16_t gid16;
    if (!rec.ReadU16(&gid16)) {
      return OTS_FAILURE_MSG("Failed to read component gid");
    }
    gid = gid16;
  }
  if (gid >= state.numGlyphs) {
    return OTS_FAILURE_MSG("Component gid %u out of range", gid);
  }

  // Condition
  if (flags & HAVE_CONDITION) {
    uint32_t conditionIndex;
    if (!ReadUint32Var(rec, &conditionIndex)) {
      return OTS_FAILURE_MSG("Failed to read component condition index");
    }
    if (conditionIndex >= state.conditionCount) {
      return OTS_FAILURE_MSG("Component condition index %u out of range",
                             conditionIndex);
    }
  }

  // Axis values
  if (flags & HAVE_AXES) {
    uint32_t axisIndicesIndex;
    if (!ReadUint32Var(rec, &axisIndicesIndex)) {
      return OTS_FAILURE_MSG("Failed to read component axis indices index");
    }
    if (axisIndicesIndex >= state.axisIndicesCounts.size()) {
      return OTS_FAILURE_MSG("Component axis indices index %u out of range",
                             axisIndicesIndex);
    }
    const size_t numAxisValues = state.axisIndicesCounts[axisIndicesIndex];
    if (!ParseTupleValues(font, rec, /*count_known=*/true, numAxisValues, NULL)) {
      return OTS_FAILURE_MSG("Failed to read component axis values");
    }
  }

  // Variation indices
  if (flags & AXIS_VALUES_HAVE_VARIATION) {
    uint32_t axisValuesVarIndex;
    if (!ReadUint32Var(rec, &axisValuesVarIndex)) {
      return OTS_FAILURE_MSG("Failed to read component axis values var index");
    }
    if (!ValidateVarIdx(font, axisValuesVarIndex, state)) {
      return OTS_FAILURE_MSG("Bad axis values var index");
    }
  }
  if (flags & TRANSFORM_HAS_VARIATION) {
    uint32_t transformVarIndex;
    if (!ReadUint32Var(rec, &transformVarIndex)) {
      return OTS_FAILURE_MSG("Failed to read component transform var index");
    }
    if (!ValidateVarIdx(font, transformVarIndex, state)) {
      return OTS_FAILURE_MSG("Bad transform var index");
    }
  }

  // Transform fields. Each present field is a single 16-bit value; they appear
  // in this fixed order (translate, rotation, scale, skew, tcenter).
  static const uint32_t kTransformFields[] = {
    HAVE_TRANSLATE_X, HAVE_TRANSLATE_Y,
    HAVE_ROTATION,
    HAVE_SCALE_X, HAVE_SCALE_Y,
    HAVE_SKEW_X, HAVE_SKEW_Y,
    HAVE_TCENTER_X, HAVE_TCENTER_Y,
  };
  for (uint32_t field : kTransformFields) {
    if ((flags & field) && !rec.Skip(2)) {
      return OTS_FAILURE_MSG("Failed to read component transform field");
    }
  }

  // Reserved: one uint32var is present for each set bit in RESERVED_MASK.
  for (uint32_t reserved = flags & RESERVED_MASK; reserved; reserved &= reserved - 1) {
    uint32_t discard;
    if (!ReadUint32Var(rec, &discard)) {
      return OTS_FAILURE_MSG("Failed to read reserved component field");
    }
  }

  return true;
}

// glyphRecords: a CFF2-style Index whose entries are VarCompositeGlyph records
// (each a concatenation of Variable Component records).
bool ParseGlyphRecords(const ots::Font* font, const uint8_t* data,
                       size_t length, const varcState& state) {
  uint32_t count = 0;
  std::vector<IndexObject> objects;
  if (!ParseCFF2Index(font, data, length, &count, &objects)) {
    return OTS_FAILURE_MSG("Failed to parse glyphRecords Index");
  }

  for (const auto& obj : objects) {
    ots::Buffer rec(data + obj.offset, obj.length);
    while (rec.remaining() > 0) {
      if (!ParseVarComponent(font, rec, state)) {
        return OTS_FAILURE_MSG("Failed to parse variable component");
      }
    }
  }

  return true;
}

}  // namespace

namespace ots {

bool OpenTypeVARC::Parse(const uint8_t* data, size_t length) {
  Font* font = GetFont();
  Buffer table(data, length);

  const size_t headerSize = 24;

  uint16_t majorVersion, minorVersion;
  uint32_t coverageOffset, varStoreOffset, conditionListOffset,
           axisIndicesListOffset, glyphRecordsOffset;
  if (!table.ReadU16(&majorVersion) ||
      !table.ReadU16(&minorVersion) ||
      !table.ReadU32(&coverageOffset) ||
      !table.ReadU32(&varStoreOffset) ||
      !table.ReadU32(&conditionListOffset) ||
      !table.ReadU32(&axisIndicesListOffset) ||
      !table.ReadU32(&glyphRecordsOffset)) {
    return Error("Incomplete table");
  }

  if (majorVersion != 1) {
    return Error("Unknown VARC table major version %u", majorVersion);
  }

  varcState state;

  auto* maxp = static_cast<OpenTypeMAXP*>(font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table missing");
  }
  state.numGlyphs = maxp->num_glyphs;

  // fvar is optional: a static font may use VARC without variation axes.
  auto* fvar = static_cast<OpenTypeFVAR*>(font->GetTypedTable(OTS_TAG_FVAR));
  state.axisCount = fvar ? fvar->AxisCount() : 0;

  // Coverage and glyphRecords are required; the others may be NULL.
  if (coverageOffset < headerSize || coverageOffset >= length) {
    return Error("Bad coverage offset");
  }
  if (glyphRecordsOffset < headerSize || glyphRecordsOffset >= length) {
    return Error("Bad glyphRecords offset");
  }
  if (varStoreOffset &&
      (varStoreOffset < headerSize || varStoreOffset >= length)) {
    return Error("Bad varStore offset");
  }
  if (conditionListOffset &&
      (conditionListOffset < headerSize || conditionListOffset >= length)) {
    return Error("Bad conditionList offset");
  }
  if (axisIndicesListOffset &&
      (axisIndicesListOffset < headerSize || axisIndicesListOffset >= length)) {
    return Error("Bad axisIndicesList offset");
  }

  // Parse the variation store first: conditions and glyph components refer to
  // it by VarIdx, and glyph components refer to the axisIndicesList.
  if (varStoreOffset) {
    if (!ParseMultiItemVariationStore(font, data + varStoreOffset,
                                      length - varStoreOffset, &state)) {
      return Error("Failed to parse MultiItemVariationStore");
    }
  }

  if (conditionListOffset) {
    if (!ParseConditionList(font, data + conditionListOffset,
                            length - conditionListOffset, &state)) {
      return Error("Failed to parse ConditionList");
    }
  }

  if (axisIndicesListOffset) {
    if (!ParseAxisIndicesList(font, data + axisIndicesListOffset,
                              length - axisIndicesListOffset, &state)) {
      return Error("Failed to parse axisIndicesList");
    }
  }

  if (!ParseCoverageTable(font, data + coverageOffset, length - coverageOffset,
                          state.numGlyphs)) {
    return Error("Failed to parse coverage table");
  }

  if (!ParseGlyphRecords(font, data + glyphRecordsOffset,
                         length - glyphRecordsOffset, state)) {
    return Error("Failed to parse glyphRecords");
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeVARC::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write VARC table");
  }
  return true;
}

}  // namespace ots

#undef TABLE_NAME
