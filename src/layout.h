// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_LAYOUT_H_
#define OTS_LAYOUT_H_

#include "ots.h"

// Utility functions for OpenType layout common table formats.
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace ots {

// The maximum number of class value.
const uint16_t kMaxClassDefValue = 0xFFFF;

class OpenTypeLayoutTable : public Table {
  public:
    explicit OpenTypeLayoutTable(Font *font, uint32_t tag, uint32_t type)
      : Table(font, tag, type) { }

    bool Parse(const uint8_t *data, size_t length);
    bool Serialize(OTSStream *out);

  protected:
    bool ParseContextSubtable(const uint8_t *data, const size_t length);
    bool ParseChainingContextSubtable(const uint8_t *data, const size_t length);
    bool ParseExtensionSubtable(const uint8_t *data, const size_t length);

  private:
    bool ParseScriptListTable(const uint8_t *data, const size_t length);
    bool ParseFeatureListTable(const uint8_t *data, const size_t length);
    bool ParseLookupListTable(const uint8_t *data, const size_t length);
    bool ParseFeatureVariationsTable(const uint8_t *data, const size_t length);
    bool ParseLookupTable(const uint8_t *data, const size_t length);

    virtual bool ValidLookupSubtableType(const uint16_t lookup_type,
                                         bool extension = false) const = 0;
    virtual bool ParseLookupSubtable(const uint8_t *data, const size_t length,
                                     const uint16_t lookup_type) = 0;

    const uint8_t *m_data = nullptr;
    size_t m_length = 0;
    uint16_t m_num_features = 0;
    uint16_t m_num_lookups = 0;
};

bool ParseClassDefTable(const ots::Font *font,
                        const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes);

bool ParseCoverageTable(const ots::Font *font,
                        const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t expected_num_glyphs = 0);

bool ParseDeviceTable(const ots::Font *font,
                      const uint8_t *data, size_t length);

// Layout tables (GSUB, GPOS, GDEF, MATH) are trees of offsets, and nothing
// prevents many offsets from pointing at the same subtable. Since we validate
// the target of every offset we follow, without a bound the work needed to
// check a table is the product of its nested array counts rather than their
// sum, so a table of a few kilobytes can take minutes to parse. To bound this,
// each table gets a budget of work units proportional to its length; the
// parsers charge the number of records they are about to process and the table
// is rejected once the budget runs out.
//
// The constants mirror HarfBuzz's sanitizer (hb-sanitize.hh:
// HB_SANITIZE_MAX_OPS_FACTOR/MIN/MAX): a budget of 64 units per byte, clamped
// to [16384, 0x3FFFFFFF]. A well-formed table re-reads each of its bytes only a
// few times and stays far under the budget (large production faces such as
// Gulzar and Duployan use well under 1 unit per byte). A table that exceeds the
// budget is re-validating some subtable many times over, which is the pattern
// HarfBuzz's own sanitizer refuses to parse -- so a font rejected here is one
// whose table the shaper would not have accepted either. We charge record
// counts where HarfBuzz charges bytes; the two differ only by each record's
// width, well within the 64x factor, so this stays in step with HarfBuzz for
// real fonts rather than being an independent, stricter limit.
const uint64_t kLayoutWorkPerByte = 64;
const uint64_t kLayoutMinWorkBudget = 16384;
const uint64_t kLayoutMaxWorkBudget = 0x3FFFFFFF;

inline void ResetLayoutWorkBudget(const ots::Font *font, size_t table_length) {
  uint64_t budget = kLayoutWorkPerByte * static_cast<uint64_t>(table_length);
  if (budget < kLayoutMinWorkBudget) {
    budget = kLayoutMinWorkBudget;
  } else if (budget > kLayoutMaxWorkBudget) {
    budget = kLayoutMaxWorkBudget;
  }
  font->layout_work_budget = budget;
}

inline bool ChargeLayoutWork(const ots::Font *font, uint64_t units) {
  if (units > font->layout_work_budget) {
    font->layout_work_budget = 0;
    return false;
  }
  font->layout_work_budget -= units;
  return true;
}

}  // namespace ots

#endif  // OTS_LAYOUT_H_

