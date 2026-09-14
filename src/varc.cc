// Copyright (c) 2025 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "varc.h"

#include "cff.h"
#include "fvar.h"
#include "gvar.h"
#include "layout.h"
#include "maxp.h"

#include <map>
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

// Tuple length recorded for the delta sets of a MultiItemVariationData that
// has no regions: such delta sets contribute nothing, so any expected tuple
// length is acceptable.
const uint32_t kAnyTupleLength = 0xFFFFFFFFu;

// State collected while parsing, used to validate cross-references between the
// various sub-tables.
struct varcState {
  uint16_t numGlyphs = 0;      // from maxp
  uint16_t axisCount = 0;      // from fvar or the base outline variation data

  // MultiItemVariationStore tuple lengths. Multiple outer indices may use the
  // same subtable offset, so keep one vector per distinct subtable and map each
  // outer index to it.
  std::vector<std::vector<uint32_t>> uniqueDeltaSetTupleLengths;
  std::vector<uint32_t> outerToTupleLengths;

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
  // 5-byte form. The low nibble of the lead byte contributes bits at or above
  // 32, which do not fit in a uint32, so the only valid lead byte is 0xF0.
  if (b0 != 0xF0) {
    return false;
  }
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
// decoded until |buf| is exhausted. *out_count (if non-NULL) receives the
// number of values decoded. Optional range and count limits avoid expanding
// compact zero runs into attacker-controlled amounts of temporary storage.
bool ParseTupleValues(const ots::Font* font, ots::Buffer& buf, bool count_known,
                      size_t known_count, size_t* out_count,
                      bool validate_range = false, int32_t min_value = 0,
                      int32_t max_value = 0,
                      size_t max_count = std::numeric_limits<size_t>::max()) {
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
    if (run > max_count - total) {
      return OTS_FAILURE_MSG("TupleValues has too many values");
    }

    if ((control & VALUES_SIZE_MASK) == VALUES_ARE_ZEROS) {
      if (validate_range && (0 < min_value || 0 > max_value)) {
        return OTS_FAILURE_MSG("TupleValues value out of range");
      }
      total += run;
      continue;
    }

    for (size_t i = 0; i < run; ++i) {
      int32_t value = 0;
      bool ok = true;
      switch (control & VALUES_SIZE_MASK) {
        case VALUES_ARE_BYTES: {
          uint8_t v;
          ok = buf.ReadU8(&v);
          value = static_cast<int8_t>(v);
          break;
        }
        case VALUES_ARE_WORDS: {
          uint16_t v;
          ok = buf.ReadU16(&v);
          value = static_cast<int16_t>(v);
          break;
        }
        case VALUES_ARE_LONGS: {
          uint32_t v;
          ok = buf.ReadU32(&v);
          value = static_cast<int32_t>(v);
          break;
        }
        case VALUES_ARE_ZEROS:
          return OTS_FAILURE_MSG("Unexpected zero TupleValues run");
      }
      if (!ok) {
        return OTS_FAILURE_MSG("Failed to read TupleValues data");
      }
      if (validate_range && (value < min_value || value > max_value)) {
        return OTS_FAILURE_MSG("TupleValues value %d out of range", value);
      }
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

// A single MultiItemVariationData sub-table. *out_tupleLengths receives the
// tuple length of each delta set it stores (indexed by "inner" index).
bool ParseMultiItemVariationData(const ots::Font* font, const uint8_t* data,
                                 size_t length, uint16_t regionCount,
                                 std::vector<uint32_t>* out_tupleLengths) {
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
  uint32_t deltaSetCount = 0;
  std::vector<IndexObject> deltaSets;
  if (!ParseCFF2Index(font, data + indexStart, length - indexStart,
                      &deltaSetCount, &deltaSets)) {
    return OTS_FAILURE_MSG("Failed to parse delta sets Index");
  }

  // Each delta set is the concatenation of one tuple of values per region, so
  // its value count must be a multiple of the region count; the quotient is
  // the tuple length, which users of the delta set must agree with.
  out_tupleLengths->clear();
  out_tupleLengths->reserve(deltaSets.size());
  for (size_t i = 0; i < deltaSets.size(); ++i) {
    ots::Buffer deltaSet(data + indexStart + deltaSets[i].offset,
                         deltaSets[i].length);
    size_t numValues = 0;
    if (!ParseTupleValues(font, deltaSet, /*count_known=*/false, 0, &numValues,
                          false, 0, 0,
                          std::numeric_limits<uint32_t>::max())) {
      return OTS_FAILURE_MSG("Failed to parse delta set %u",
                             static_cast<unsigned>(i));
    }
    if (regionIndexCount == 0) {
      if (numValues != 0) {
        return OTS_FAILURE_MSG("Delta set %u has values but no regions",
                               static_cast<unsigned>(i));
      }
      out_tupleLengths->push_back(kAnyTupleLength);
      continue;
    }
    if (numValues % regionIndexCount != 0) {
      return OTS_FAILURE_MSG("Delta set %u has %u values, not a multiple of "
                             "the region count %u",
                             static_cast<unsigned>(i),
                             static_cast<unsigned>(numValues),
                             regionIndexCount);
    }
    out_tupleLengths->push_back(
        static_cast<uint32_t>(numValues / regionIndexCount));
  }

  return true;
}

// MultiItemVariationStore. Fills state.deltaSetTupleLengths.
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

  state->uniqueDeltaSetTupleLengths.clear();
  state->outerToTupleLengths.clear();
  std::map<uint32_t, uint32_t> parsedOffsets;
  for (unsigned i = 0; i < dataCount; ++i) {
    uint32_t offset;
    if (!subtable.ReadU32(&offset)) {
      return OTS_FAILURE_MSG("Failed to read item variation data offset");
    }
    if (offset < headerEnd || offset >= length) {
      return OTS_FAILURE_MSG("Bad item variation data offset");
    }
    auto parsed = parsedOffsets.find(offset);
    uint32_t tupleLengthsIndex;
    if (parsed != parsedOffsets.end()) {
      tupleLengthsIndex = parsed->second;
    } else {
      std::vector<uint32_t> tupleLengths;
      if (!ParseMultiItemVariationData(font, data + offset, length - offset,
                                       regionCount, &tupleLengths)) {
        return OTS_FAILURE_MSG("Failed to parse item variation data %u", i);
      }
      tupleLengthsIndex = static_cast<uint32_t>(
          state->uniqueDeltaSetTupleLengths.size());
      state->uniqueDeltaSetTupleLengths.push_back(std::move(tupleLengths));
      parsedOffsets[offset] = tupleLengthsIndex;
    }
    state->outerToTupleLengths.push_back(tupleLengthsIndex);
  }

  return true;
}

// 0xFFFFFFFF is a sentinel VarIdx meaning "no variation"; both HarfBuzz and
// fonttools use it (fonttools' variation-store builder emits it for all-zero
// delta sets, e.g. in a ConditionValue).
const uint32_t kNoVariationIndex = 0xFFFFFFFFu;

// Validate a VarIdx (outer index in the top 16 bits, inner in the low 16 bits)
// against the MultiItemVariationStore. The delta set it refers to must supply
// |tupleLength| values per region, as that is how many the user will read.
bool ValidateVarIdx(const ots::Font* font, uint32_t varIdx,
                    const varcState& state, uint32_t tupleLength) {
  if (varIdx == kNoVariationIndex) {
    return true;
  }
  const uint16_t outer = varIdx >> 16;
  const uint16_t inner = varIdx & 0xFFFFu;
  if (outer >= state.outerToTupleLengths.size()) {
    return OTS_FAILURE_MSG("VarIdx outer index %u out of range", outer);
  }
  const std::vector<uint32_t>& tupleLengths =
      state.uniqueDeltaSetTupleLengths[state.outerToTupleLengths[outer]];
  if (inner >= tupleLengths.size()) {
    return OTS_FAILURE_MSG("VarIdx inner index %u out of range", inner);
  }
  if (tupleLengths[inner] != kAnyTupleLength &&
      tupleLengths[inner] != tupleLength) {
    return OTS_FAILURE_MSG("VarIdx %u refers to a delta set of tuple length %u, "
                           "expected %u", varIdx, tupleLengths[inner],
                           tupleLength);
  }
  return true;
}

// Conditions may nest (formats 3, 4 and 5); bound the recursion.
const uint32_t kConditionRecursionLimit = 64;

// A ConditionList together with, for each byte offset into it, whether the
// condition starting there has already been validated. Child conditions may be
// shared between parents; validating each one only once keeps the total work
// linear in the list's size rather than exponential in the nesting depth.
struct conditionContext {
  const uint8_t* data;
  size_t length;
  // Zero means unvalidated; other values are one plus the maximum number of
  // child edges below the condition at that byte offset.
  std::vector<uint16_t> subtreeHeights;
};

bool ParseCondition(const ots::Font* font, conditionContext& ctx,
                    size_t offset, const varcState& state, uint32_t depth,
                    uint32_t* outHeight);

// Validate the condition at |offset| into the list (known to be in bounds).
// Offsets to child conditions are relative to the condition containing them.
bool ParseConditionUncached(const ots::Font* font, conditionContext& ctx,
                            size_t offset, const varcState& state,
                            uint32_t depth, uint32_t* outHeight) {
  ots::Buffer subtable(ctx.data + offset, ctx.length - offset);
  const size_t length = subtable.remaining();

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
      // The range is in normalized F2Dot14 coordinates, so within [-1, 1].
      if (filterRangeMin < -0x4000 || filterRangeMax > 0x4000) {
        return OTS_FAILURE_MSG("Condition filter range out of range");
      }
      if (filterRangeMin > filterRangeMax) {
        OTS_WARNING("Misordered filter range in condition");
      }
      *outHeight = 0;
      return true;
    }

    case 2: {  // ConditionValue (uses the variation store)
      int16_t defaultValue;
      uint32_t varIdx;
      if (!subtable.ReadS16(&defaultValue) || !subtable.ReadU32(&varIdx)) {
        return OTS_FAILURE_MSG("Failed to read condition format 2");
      }
      if (!ValidateVarIdx(font, varIdx, state, /*tupleLength=*/1)) {
        return OTS_FAILURE_MSG("Bad VarIdx in condition format 2");
      }
      *outHeight = 0;
      return true;
    }

    case 3:    // ConditionAnd
    case 4: {  // ConditionOr
      uint8_t conditionCount;
      if (!subtable.ReadU8(&conditionCount)) {
        return OTS_FAILURE_MSG("Failed to read condition format %u count", format);
      }
      uint32_t height = 0;
      for (unsigned i = 0; i < conditionCount; ++i) {
        uint32_t childOffset;
        if (!subtable.ReadU24(&childOffset)) {
          return OTS_FAILURE_MSG("Failed to read child condition offset");
        }
        if (childOffset < 2u || childOffset >= length) {
          return OTS_FAILURE_MSG("Bad child condition offset");
        }
        uint32_t childHeight = 0;
        if (!ParseCondition(font, ctx, offset + childOffset, state, depth + 1,
                            &childHeight)) {
          return OTS_FAILURE_MSG("Failed to parse child condition");
        }
        if (childHeight + 1 > height) {
          height = childHeight + 1;
        }
      }
      *outHeight = height;
      return true;
    }

    case 5: {  // ConditionNegate
      uint32_t childOffset;
      if (!subtable.ReadU24(&childOffset)) {
        return OTS_FAILURE_MSG("Failed to read negated condition offset");
      }
      if (childOffset < 2u || childOffset >= length) {
        return OTS_FAILURE_MSG("Bad negated condition offset");
      }
      uint32_t childHeight = 0;
      if (!ParseCondition(font, ctx, offset + childOffset, state, depth + 1,
                          &childHeight)) {
        return OTS_FAILURE_MSG("Failed to parse negated condition");
      }
      *outHeight = childHeight + 1;
      return true;
    }

    default:
      return OTS_FAILURE_MSG("Unknown condition format: %u", format);
  }
}

bool ParseCondition(const ots::Font* font, conditionContext& ctx,
                    size_t offset, const varcState& state, uint32_t depth,
                    uint32_t* outHeight) {
  if (depth > kConditionRecursionLimit) {
    return OTS_FAILURE_MSG("Excessive condition nesting");
  }
  if (ctx.subtreeHeights[offset]) {
    const uint32_t height = ctx.subtreeHeights[offset] - 1;
    if (height > kConditionRecursionLimit - depth) {
      return OTS_FAILURE_MSG("Excessive condition nesting");
    }
    *outHeight = height;
    return true;
  }
  uint32_t height = 0;
  if (!ParseConditionUncached(font, ctx, offset, state, depth, &height)) {
    return false;
  }
  ctx.subtreeHeights[offset] = static_cast<uint16_t>(height + 1);
  *outHeight = height;
  return true;
}

bool ParseConditionList(const ots::Font* font, const uint8_t* data,
                        size_t length, varcState* state) {
  ots::Buffer subtable(data, length);

  uint32_t conditionCount;
  if (!subtable.ReadU32(&conditionCount)) {
    return OTS_FAILURE_MSG("Failed to read condition list count");
  }
  state->conditionCount = conditionCount;

  conditionContext ctx;
  ctx.data = data;
  ctx.length = length;
  ctx.subtreeHeights.resize(length);

  for (unsigned i = 0; i < conditionCount; ++i) {
    uint32_t offset;
    if (!subtable.ReadU32(&offset)) {
      return OTS_FAILURE_MSG("Failed to read condition offset");
    }
    if (offset < 4u || offset >= length) {
      return OTS_FAILURE_MSG("Bad condition offset");
    }
    uint32_t height = 0;
    if (!ParseCondition(font, ctx, offset, *state, 0, &height)) {
      return OTS_FAILURE_MSG("Failed to parse condition %u", i);
    }
  }

  return true;
}

// axisIndicesList: a CFF2-style Index whose entries are TupleValues encoding the
// axis indices used by glyph components. Each index must name an fvar axis.
// Fills state.axisIndicesCounts with the number of values in each entry.
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
    size_t axisIndicesCount = 0;
    if (!ParseTupleValues(font, entry, /*count_known=*/false, 0,
                          &axisIndicesCount, true, 0,
                          static_cast<int32_t>(state->axisCount) - 1,
                          state->axisCount)) {
      return OTS_FAILURE_MSG("Failed to parse axisIndices entry");
    }
    state->axisIndicesCounts.push_back(
        static_cast<uint32_t>(axisIndicesCount));
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
  size_t numAxisValues = 0;
  if (flags & HAVE_AXES) {
    uint32_t axisIndicesIndex;
    if (!ReadUint32Var(rec, &axisIndicesIndex)) {
      return OTS_FAILURE_MSG("Failed to read component axis indices index");
    }
    if (axisIndicesIndex >= state.axisIndicesCounts.size()) {
      return OTS_FAILURE_MSG("Component axis indices index %u out of range",
                             axisIndicesIndex);
    }
    numAxisValues = state.axisIndicesCounts[axisIndicesIndex];
    if (!ParseTupleValues(font, rec, /*count_known=*/true, numAxisValues, NULL,
                          true, -0x4000, 0x4000, numAxisValues)) {
      return OTS_FAILURE_MSG("Failed to read component axis values");
    }
  }

  // Transform fields. Each present field is a single 16-bit value; they appear
  // in this fixed order (translate, rotation, scale, skew, tcenter). The
  // transform delta sets carry one value per present field.
  static const uint32_t kTransformFields[] = {
    HAVE_TRANSLATE_X, HAVE_TRANSLATE_Y,
    HAVE_ROTATION,
    HAVE_SCALE_X, HAVE_SCALE_Y,
    HAVE_SKEW_X, HAVE_SKEW_Y,
    HAVE_TCENTER_X, HAVE_TCENTER_Y,
  };
  uint32_t numTransformFields = 0;
  for (uint32_t field : kTransformFields) {
    if (flags & field) {
      ++numTransformFields;
    }
  }

  // Variation indices
  if (flags & AXIS_VALUES_HAVE_VARIATION) {
    uint32_t axisValuesVarIndex;
    if (!ReadUint32Var(rec, &axisValuesVarIndex)) {
      return OTS_FAILURE_MSG("Failed to read component axis values var index");
    }
    if (!ValidateVarIdx(font, axisValuesVarIndex, state,
                        static_cast<uint32_t>(numAxisValues))) {
      return OTS_FAILURE_MSG("Bad axis values var index");
    }
  }
  if (flags & TRANSFORM_HAS_VARIATION) {
    uint32_t transformVarIndex;
    if (!ReadUint32Var(rec, &transformVarIndex)) {
      return OTS_FAILURE_MSG("Failed to read component transform var index");
    }
    if (!ValidateVarIdx(font, transformVarIndex, state, numTransformFields)) {
      return OTS_FAILURE_MSG("Bad transform var index");
    }
  }

  // Transform fields.
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
// (each a concatenation of Variable Component records). *out_count receives the
// number of records.
bool ParseGlyphRecords(const ots::Font* font, const uint8_t* data,
                       size_t length, const varcState& state,
                       uint32_t* out_count) {
  uint32_t count = 0;
  std::vector<IndexObject> objects;
  if (!ParseCFF2Index(font, data, length, &count, &objects)) {
    return OTS_FAILURE_MSG("Failed to parse glyphRecords Index");
  }
  *out_count = count;

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

  // fvar is optional.  A static VARC font may use the axes internal to gvar or
  // CFF2 without exposing them as user-selectable design axes.
  auto* fvar = static_cast<OpenTypeFVAR*>(font->GetTypedTable(OTS_TAG_FVAR));
  if (fvar) {
    state.axisCount = fvar->AxisCount();
  } else if (auto* gvar = static_cast<OpenTypeGVAR*>(
                 font->GetTypedTable(OTS_TAG_GVAR))) {
    state.axisCount = gvar->AxisCount();
  } else if (auto* cff2 = static_cast<OpenTypeCFF2*>(
                 font->GetTypedTable(OTS_TAG_CFF2))) {
    state.axisCount = cff2->variation_axis_count;
  }

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

  uint32_t glyphRecordCount = 0;
  if (!ParseGlyphRecords(font, data + glyphRecordsOffset,
                         length - glyphRecordsOffset, state,
                         &glyphRecordCount)) {
    return Error("Failed to parse glyphRecords");
  }

  // The coverage table's glyphs index the glyphRecords sequentially, so their
  // counts must agree. Coverage cannot express more than 0xFFFF glyphs.
  if (glyphRecordCount > 0xFFFF) {
    return Error("Too many glyph records: %u", glyphRecordCount);
  }
  uint32_t coverageGlyphCount = 0;
  uint16_t lastCoveredGlyph = 0;
  if (!ParseCoverageTable(font, data + coverageOffset,
                          length - coverageOffset, state.numGlyphs, 0,
                          &coverageGlyphCount, &lastCoveredGlyph)) {
    return Error("Failed to parse coverage table");
  }
  if (coverageGlyphCount != glyphRecordCount) {
    return Error("Coverage and glyphRecords counts differ");
  }
  if (coverageGlyphCount && lastCoveredGlyph >= state.numGlyphs) {
    return Error("Covered glyph %u out of range", lastCoveredGlyph);
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
