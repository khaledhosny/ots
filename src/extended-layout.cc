// Copyright (c) 2026 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout.h"

#include <vector>

#include "maxp.h"

namespace {

// OFF's extended lookup formats mix 16-bit sequence lengths, 24-bit glyphs
// and local offsets, and 32-bit coverage/class-definition offsets. Keep the
// widths explicit, rather than promoting every field in a legacy structure.
class Reader : public ots::Buffer {
 public:
  Reader(const uint8_t* data, size_t length) : Buffer(data, length) {}

  bool U16(uint32_t& value) {
    uint16_t v;
    if (!ReadU16(&v)) return false;
    value = v;
    return true;
  }
  bool U24(uint32_t& value) { return ReadU24(&value); }
  bool U32(uint32_t& value) { return ReadU32(&value); }

  bool Offset(uint32_t offset, size_t end, bool nullable = false) const {
    return (nullable && !offset) || (offset >= end && offset < length());
  }

  bool Offsets(uint32_t count, unsigned width, std::vector<uint32_t>& offsets,
               bool nullable = false, size_t following = 0) {
    const uint64_t end = offset() + static_cast<uint64_t>(count) * width + following;
    if (end > length()) return false;
    offsets.resize(count);
    for (auto& offset : offsets) {
      if (!(width == 2 ? U16(offset) : width == 3 ? U24(offset) : U32(offset)) ||
          !Offset(offset, end, nullable)) return false;
    }
    return true;
  }
};

class ExtendedLookup {
 public:
  ExtendedLookup(const ots::Font* font, uint16_t num_lookups, size_t length)
      : font_(font), glyph_count_(font->GetMaxp()->num_glyphs),
        lookup_count_(num_lookups), budget_(length * 4 + 1024) {}

  bool Substitution(Reader& r, uint16_t type, uint32_t format);
  bool Context(Reader& r, uint32_t format, bool chained);

 private:
  // Shared offsets can otherwise make validation quadratic in table length.
  bool Work(size_t count = 1) {
    if (count > budget_) return false;
    budget_ -= count;
    return true;
  }

  bool Coverage(const Reader& r, uint32_t offset, uint32_t expected = UINT32_MAX) {
    uint32_t count = 0;
    return r.Offset(offset, r.offset()) &&
        ots::ParseCoverageTable(font_, r.buffer() + offset, r.length() - offset,
                                glyph_count_, 0, &count) &&
        (expected == UINT32_MAX || expected == count) && Work(count);
  }

  bool Classes(const Reader& r, uint32_t offset, uint32_t count) {
    return count && r.Offset(offset, r.offset()) &&
        ots::ParseClassDefTable(font_, r.buffer() + offset, r.length() - offset,
                                glyph_count_, count - 1);
  }

  bool Glyphs(Reader& r, uint32_t count, bool classes = false,
              uint32_t class_count = 65536) {
    if (!Work(count) || count > r.remaining() / (classes ? 2 : 3)) return false;
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t value;
      if (!(classes ? r.U16(value) : r.U24(value)) ||
          value >= (classes ? class_count : glyph_count_)) return false;
    }
    return true;
  }

  bool Lookups(Reader& r, uint32_t count, uint32_t input_count) {
    if (!Work(count) || count > r.remaining() / 4) return false;
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t sequence, lookup;
      if (!r.U16(sequence) || !r.U16(lookup) ||
          sequence >= input_count || lookup >= lookup_count_) return false;
    }
    return true;
  }

  bool Rule(Reader& r, bool chained, bool classes, uint32_t class_count);
  bool RuleSet(Reader& r, bool chained, bool classes, uint32_t class_count);
  bool Delta(const Reader& r, uint32_t coverage, uint32_t delta);

  const ots::Font* font_;
  uint32_t glyph_count_;
  uint16_t lookup_count_;
  size_t budget_;
};

bool ExtendedLookup::Delta(const Reader& r, uint32_t coverage, uint32_t delta) {
  Reader c(r.buffer() + coverage, r.length() - coverage);
  uint32_t format, count;
  if (!c.U16(format) || !(format > 2 ? c.U24(count) : c.U16(count))) return false;
  const bool wide = format > 2;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t start, end, index;
    if (!Work() || !(wide ? c.U24(start) : c.U16(start))) return false;
    end = start;
    if (!(format & 1) &&
        (!(wide ? c.U24(end) : c.U16(end)) ||
         !(wide ? c.U24(index) : c.U16(index)))) return false;
    start = (start + delta) & 0xffffff;
    end = (end + delta) & 0xffffff;
    if (start > end || end >= glyph_count_) return false;
  }
  return true;
}

bool ExtendedLookup::Substitution(Reader& r, uint16_t type, uint32_t format) {
  uint32_t coverage;
  if (!r.U32(coverage)) return false;
  if (type == 1) {
    uint32_t value;
    if (!r.U24(value)) return false;
    if (format == 3) return Coverage(r, coverage) && Delta(r, coverage, value);
    if (format != 4 || !Glyphs(r, value)) return false;
    return Coverage(r, coverage, value);
  }

  if (type == 8) {
    if (format != 2) return false;
    std::vector<uint32_t> backtrack, lookahead;
    uint32_t count;
    if (!r.U16(count) || !r.Offsets(count, 3, backtrack) ||
        !r.U16(count) || !r.Offsets(count, 3, lookahead) ||
        !r.U24(count) || !Glyphs(r, count) || !Coverage(r, coverage, count)) return false;
    for (uint32_t offset : backtrack) if (!Coverage(r, offset)) return false;
    for (uint32_t offset : lookahead) if (!Coverage(r, offset)) return false;
    return true;
  }

  if (format != 2 || type < 2 || type > 4) return false;
  uint32_t count;
  std::vector<uint32_t> offsets;
  if (!r.U24(count) || !r.Offsets(count, 3, offsets) ||
      !Coverage(r, coverage, count)) return false;
  for (uint32_t offset : offsets) {
    if (!Work()) return false;
    Reader child(r.buffer() + offset, r.length() - offset);
    uint32_t child_count;
    if (!child.U16(child_count)) return false;
    if (type != 4) {
      if (!Glyphs(child, child_count)) return false;
      continue;
    }
    std::vector<uint32_t> ligatures;
    if (!child.Offsets(child_count, 3, ligatures)) return false;
    for (uint32_t ligature : ligatures) {
      if (!Work()) return false;
      Reader lig(child.buffer() + ligature, child.length() - ligature);
      uint32_t glyph, components;
      if (!lig.U24(glyph) || glyph >= glyph_count_ ||
          !lig.U16(components) || !components || !Glyphs(lig, components - 1)) return false;
    }
  }
  return true;
}

bool ExtendedLookup::Rule(Reader& r, bool chained, bool classes, uint32_t class_count) {
  uint32_t count, inputs, lookups;
  if (chained) {
    if (!r.U16(count) || !Glyphs(r, count, classes) ||
        !r.U16(inputs) || !inputs || !Glyphs(r, inputs - 1, classes, class_count) ||
        !r.U16(count) || !Glyphs(r, count, classes) || !r.U16(lookups)) return false;
  } else {
    if (!r.U16(inputs) || !inputs || !r.U16(lookups) ||
        !Glyphs(r, inputs - 1, classes, class_count)) return false;
  }
  return Lookups(r, lookups, inputs);
}

bool ExtendedLookup::RuleSet(Reader& r, bool chained, bool classes, uint32_t class_count) {
  uint32_t count;
  std::vector<uint32_t> offsets;
  // SequenceRuleSet2 deliberately retains Offset16; the other extended
  // rule sets have Offset24 references.
  if (!r.U16(count) || !r.Offsets(count, chained || classes ? 3 : 2, offsets)) return false;
  for (uint32_t offset : offsets) {
    if (!Work()) return false;
    Reader rule(r.buffer() + offset, r.length() - offset);
    if (!Rule(rule, chained, classes, class_count)) return false;
  }
  return true;
}

bool ExtendedLookup::Context(Reader& r, uint32_t format, bool chained) {
  if (format == 6 && !chained) {
    uint32_t inputs, lookups;
    std::vector<uint32_t> coverages;
    if (!r.U16(inputs) || !inputs || !r.U16(lookups) ||
        !r.Offsets(inputs, 3, coverages, false, lookups * 4) ||
        !Lookups(r, lookups, inputs)) return false;
    for (uint32_t offset : coverages) if (!Coverage(r, offset)) return false;
    return true;
  }
  if (format != 4 && format != 5) return false;
  uint32_t coverage, back = 0, input = 0, ahead = 0, count;
  const bool classes = format == 5;
  if (!r.U32(coverage)) return false;
  if (classes) {
    if (chained && !r.U32(back)) return false;
    if (!r.U32(input)) return false;
    if (chained && !r.U32(ahead)) return false;
  }
  if (!(chained && classes ? r.U16(count) : r.U24(count))) return false;
  std::vector<uint32_t> offsets;
  if (!r.Offsets(count, 3, offsets, true) ||
      !Coverage(r, coverage, classes ? UINT32_MAX : count)) return false;
  if (classes && (!Classes(r, input, count) ||
      (chained && (!Classes(r, back, 65536) || !Classes(r, ahead, 65536))))) return false;
  for (uint32_t offset : offsets) {
    if (!offset) continue;
    if (!Work()) return false;
    Reader rules(r.buffer() + offset, r.length() - offset);
    if (!RuleSet(rules, chained, classes, count)) return false;
  }
  return true;
}

}  // namespace

namespace ots {

bool ParseExtendedLayoutSubtable(const Font* font, const uint8_t* data,
                                 size_t length, uint16_t type,
                                 uint16_t num_lookups, bool positioning) {
  if (!font->GetMaxp()) return false;
  Reader r(data, length);
  uint32_t format;
  if (!r.U16(format)) return false;
  ExtendedLookup parser(font, num_lookups, length);
  if (type == (positioning ? 7 : 5)) return parser.Context(r, format, false);
  if (type == (positioning ? 8 : 6)) return parser.Context(r, format, true);
  return !positioning && parser.Substitution(r, type, format);
}

}  // namespace ots
