// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout.h"

#include <limits>
#include <vector>

#include "gdef.h"

// OpenType Layout Common Table Formats
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace {

// The 'DFLT' tag of script table.
const uint32_t kScriptTableTagDflt = 0x44464c54;
// The value which represents there is no required feature index.
const uint16_t kNoRequiredFeatureIndexDefined = 0xFFFF;
// The lookup flag bit which indicates existence of MarkFilteringSet.
const uint16_t kUseMarkFilteringSetBit = 0x0010;
// The mask for MarkAttachmentType.
const uint16_t kMarkAttachmentTypeMask = 0xFF00;
// The maximum type number of format for device tables.
const uint16_t kMaxDeltaFormatType = 3;

struct ScriptRecord {
  uint32_t tag;
  uint16_t offset;
};

struct LangSysRecord {
  uint32_t tag;
  uint16_t offset;
};

struct FeatureRecord {
  uint32_t tag;
  uint16_t offset;
};

bool ParseLangSysTable(ots::Buffer *subtable, const uint32_t tag,
                       const uint16_t num_features) {
  uint16_t offset_lookup_order = 0;
  uint16_t req_feature_index = 0;
  uint16_t feature_count = 0;
  if (!subtable->ReadU16(&offset_lookup_order) ||
      !subtable->ReadU16(&req_feature_index) ||
      !subtable->ReadU16(&feature_count)) {
    return OTS_FAILURE();
  }
  // |offset_lookup_order| is reserved and should be NULL.
  if (offset_lookup_order != 0) {
    return OTS_FAILURE();
  }
  if (req_feature_index != kNoRequiredFeatureIndexDefined &&
      req_feature_index >= num_features) {
    return OTS_FAILURE();
  }
  if (feature_count > num_features) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    uint16_t feature_index = 0;
    if (!subtable->ReadU16(&feature_index)) {
      return OTS_FAILURE();
    }
    if (feature_index >= num_features) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseScriptTable(const uint8_t *data, const size_t length,
                      const uint32_t tag, const uint16_t num_features) {
  ots::Buffer subtable(data, length);

  uint16_t offset_default_lang_sys = 0;
  uint16_t lang_sys_count = 0;
  if (!subtable.ReadU16(&offset_default_lang_sys) ||
      !subtable.ReadU16(&lang_sys_count)) {
    return OTS_FAILURE();
  }

  // The spec requires a script table for 'DFLT' tag must contain non-NULL
  // |offset_default_lang_sys| and |lang_sys_count| == 0
  if (tag == kScriptTableTagDflt &&
      (offset_default_lang_sys == 0 || lang_sys_count != 0)) {
    OTS_WARNING("DFLT table doesn't satisfy the spec.");
    return OTS_FAILURE();
  }

  const unsigned lang_sys_record_end = static_cast<unsigned>(4) +
      lang_sys_count * 6;
  if (lang_sys_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  std::vector<LangSysRecord> lang_sys_records;
  lang_sys_records.resize(lang_sys_count);
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < lang_sys_count; ++i) {
    if (!subtable.ReadU32(&lang_sys_records[i].tag) ||
        !subtable.ReadU16(&lang_sys_records[i].offset)) {
      return OTS_FAILURE();
    }
    // The record array must store the records alphabetically by tag
    if (last_tag != 0 && last_tag > lang_sys_records[i].tag) {
      return OTS_FAILURE();
    }
    if (lang_sys_records[i].offset < lang_sys_record_end ||
        lang_sys_records[i].offset >= length) {
      return OTS_FAILURE();
    }
    last_tag = lang_sys_records[i].tag;
  }

  // Check lang sys tables
  for (unsigned i = 0; i < lang_sys_count; ++i) {
    subtable.set_offset(lang_sys_records[i].offset);
    if (!ParseLangSysTable(&subtable, lang_sys_records[i].tag, num_features)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseFeatureTable(const uint8_t *data, const size_t length,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_feature_params = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&offset_feature_params) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }

  const unsigned feature_table_end = static_cast<unsigned>(4) +
      num_lookups * 2;
  if (feature_table_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  // |offset_feature_params| is generally set to NULL.
  if (offset_feature_params != 0 &&
      (offset_feature_params < feature_table_end ||
       offset_feature_params >= length)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    uint16_t lookup_index = 0;
    if (!subtable.ReadU16(&lookup_index)) {
      return OTS_FAILURE();
    }
    // lookup index starts with 0.
    if (lookup_index >= num_lookups) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool LookupTypeParserLess(const ots::LookupTypeParser& parser,
                          const uint16_t type) {
  return parser.type < type;
}

bool ParseLookupTable(ots::OpenTypeFile *file, const uint8_t *data,
                      const size_t length, const size_t num_types,
                      const ots::LookupTypeParser* parsers) {
  ots::Buffer subtable(data, length);

  uint16_t lookup_type = 0;
  uint16_t lookup_flag = 0;
  uint16_t subtable_count = 0;
  if (!subtable.ReadU16(&lookup_type) ||
      !subtable.ReadU16(&lookup_flag) ||
      !subtable.ReadU16(&subtable_count)) {
    return OTS_FAILURE();
  }

  if (lookup_type == 0 || lookup_type > num_types) {
    return OTS_FAILURE();
  }

  // Check lookup flags.
  if ((lookup_flag & kMarkAttachmentTypeMask) &&
      (!file->gdef || !file->gdef->has_mark_attachment_class_def)) {
    return OTS_FAILURE();
  }
  bool use_mark_filtering_set = false;
  if (lookup_flag & kUseMarkFilteringSetBit) {
    if (!file->gdef || !file->gdef->has_mark_glyph_sets_def) {
      return OTS_FAILURE();
    }
    use_mark_filtering_set = true;
  }

  std::vector<uint16_t> subtables;
  subtables.reserve(subtable_count);
  // If the |kUseMarkFilteringSetBit| of |lookup_flag| is set,
  // extra 2 bytes will follow after subtable offset array.
  const unsigned lookup_table_end =
      static_cast<unsigned>(use_mark_filtering_set ? 8 : 6) +
      subtable_count * 2;
  if (lookup_table_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < subtable_count; ++i) {
    if (!subtable.ReadU16(&subtables[i])) {
      return OTS_FAILURE();
    }
    if (subtables[i] < lookup_table_end || subtables[i] >= length) {
      return OTS_FAILURE();
    }
  }

  if (use_mark_filtering_set) {
    uint16_t mark_filtering_set = 0;
    if (!subtable.ReadU16(&mark_filtering_set)) {
      return OTS_FAILURE();
    }
    if (file->gdef->num_mark_glyph_sets == 0 ||
        mark_filtering_set >= file->gdef->num_mark_glyph_sets) {
      return OTS_FAILURE();
    }
  }

  // Parse lookup subtables for this lookup type.
  for (unsigned i = 0; i < subtable_count; ++i) {
    const ots::LookupTypeParser *parser =
        std::lower_bound(parsers, parsers + num_types, lookup_type,
                         LookupTypeParserLess);
    if (parser == parsers + num_types || parser->type != lookup_type ||
        !parser->parse) {
      return OTS_FAILURE();
    }
    if (!parser->parse(file, data + subtables[i], length - subtables[i])) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseClassDefFormat1(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_classes) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t start_glyph = 0;
  if (!subtable.ReadU16(&start_glyph)) {
    return OTS_FAILURE();
  }
  if (start_glyph > num_glyphs) {
    OTS_WARNING("bad start glyph ID: %u", start_glyph);
    return OTS_FAILURE();
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    OTS_WARNING("bad glyph count: %u", glyph_count);
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t class_value = 0;
    if (!subtable.ReadU16(&class_value)) {
      return OTS_FAILURE();
    }
    if (class_value == 0 || class_value > num_classes) {
      OTS_WARNING("bad class value: %u", class_value);
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseClassDefFormat2(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_classes) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t range_count = 0;
  if (!subtable.ReadU16(&range_count)) {
    return OTS_FAILURE();
  }
  if (range_count > num_glyphs) {
    OTS_WARNING("bad range count: %u", range_count);
    return OTS_FAILURE();
  }

  uint16_t last_end = 0;
  for (unsigned i = 0; i < range_count; ++i) {
    uint16_t start = 0;
    uint16_t end = 0;
    uint16_t class_value = 0;
    if (!subtable.ReadU16(&start) ||
        !subtable.ReadU16(&end) ||
        !subtable.ReadU16(&class_value)) {
      return OTS_FAILURE();
    }
    if (start > end || (last_end && start <= last_end)) {
      OTS_WARNING("glyph range is overlapping.");
      return OTS_FAILURE();
    }
    if (class_value == 0 || class_value > num_classes) {
      OTS_WARNING("bad class value: %u", class_value);
      return OTS_FAILURE();
    }
    last_end = end;
  }

  return true;
}

bool ParseCoverageFormat1(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    OTS_WARNING("bad glyph count: %u", glyph_count);
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t glyph = 0;
    if (!subtable.ReadU16(&glyph)) {
      return OTS_FAILURE();
    }
    if (glyph > num_glyphs) {
      OTS_WARNING("bad glyph ID: %u", glyph);
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseCoverageFormat2(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t range_count = 0;
  if (!subtable.ReadU16(&range_count)) {
    return OTS_FAILURE();
  }
  if (range_count > num_glyphs) {
    OTS_WARNING("bad range count: %u", range_count);
    return OTS_FAILURE();
  }
  uint16_t last_end = 0;
  uint16_t last_start_coverage_index = 0;
  for (unsigned i = 0; i < range_count; ++i) {
    uint16_t start = 0;
    uint16_t end = 0;
    uint16_t start_coverage_index = 0;
    if (!subtable.ReadU16(&start) ||
        !subtable.ReadU16(&end) ||
        !subtable.ReadU16(&start_coverage_index)) {
      return OTS_FAILURE();
    }
    if (start > end || (last_end && start <= last_end)) {
      OTS_WARNING("glyph range is overlapping.");
      return OTS_FAILURE();
    }
    if (start_coverage_index != last_start_coverage_index) {
      OTS_WARNING("bad start coverage index.");
      return OTS_FAILURE();
    }
    last_end = end;
    last_start_coverage_index += end - start + 1;
  }

  return true;
}

}  // namespace

namespace ots {

// Parsing ScriptListTable requires number of features so we need to
// parse FeatureListTable before calling this function.
bool ParseScriptListTable(const uint8_t *data, const size_t length,
                          const uint16_t num_features) {
  Buffer subtable(data, length);

  uint16_t script_count = 0;
  if (!subtable.ReadU16(&script_count)) {
    return OTS_FAILURE();
  }

  const unsigned script_record_end = static_cast<unsigned>(2) +
      script_count * 6;
  if (script_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  std::vector<ScriptRecord> script_list;
  script_list.reserve(script_count);
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < script_count; ++i) {
    if (!subtable.ReadU32(&script_list[i].tag) ||
        !subtable.ReadU16(&script_list[i].offset)) {
      return OTS_FAILURE();
    }
    // Script tags should be arranged alphabetically by tag
    if (last_tag != 0 && last_tag > script_list[i].tag) {
      return OTS_FAILURE();
    }
    last_tag = script_list[i].tag;
    if (script_list[i].offset < script_record_end ||
        script_list[i].offset >= length) {
      return OTS_FAILURE();
    }
  }

  // Check script records.
  for (unsigned i = 0; i < script_count; ++i) {
    if (!ParseScriptTable(data + script_list[i].offset,
                          length - script_list[i].offset,
                          script_list[i].tag, num_features)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

// Parsing FeatureListTable requires number of lookups so we need to parse
// LookupListTable before calling this function.
bool ParseFeatureListTable(const uint8_t *data, const size_t length,
                           const uint16_t num_lookups,
                           uint16_t* num_features) {
  Buffer subtable(data, length);

  uint16_t feature_count = 0;
  if (!subtable.ReadU16(&feature_count)) {
    return OTS_FAILURE();
  }

  std::vector<FeatureRecord> feature_records;
  feature_records.resize(feature_count);
  const unsigned feature_record_end = static_cast<unsigned>(2) +
      feature_count * 6;
  if (feature_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < feature_count; ++i) {
    if (!subtable.ReadU32(&feature_records[i].tag) ||
        !subtable.ReadU16(&feature_records[i].offset)) {
      return OTS_FAILURE();
    }
    // Feature record array should be arranged alphabetically by tag
    if (last_tag != 0 && last_tag > feature_records[i].tag) {
      return OTS_FAILURE();
    }
    last_tag = feature_records[i].tag;
    if (feature_records[i].offset < feature_record_end ||
        feature_records[i].offset >= length) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    if (!ParseFeatureTable(data + feature_records[i].offset,
                           length - feature_records[i].offset, num_lookups)) {
      return OTS_FAILURE();
    }
  }
  *num_features = feature_count;
  return true;
}

// For parsing GPOS/GSUB tables, this function should be called at first to
// obtain the number of lookups because parsing FeatureTableList requires
// the number.
bool ParseLookupListTable(OpenTypeFile *file, const uint8_t *data,
                          const size_t length, const size_t num_types,
                          const LookupTypeParser* parsers,
                          uint16_t *num_lookups) {
  Buffer subtable(data, length);

  if (!subtable.ReadU16(num_lookups)) {
    return OTS_FAILURE();
  }

  std::vector<uint16_t> lookups;
  lookups.reserve(*num_lookups);
  const unsigned lookup_end = static_cast<unsigned>(2) +
      (*num_lookups) * 2;
  if (lookup_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < *num_lookups; ++i) {
    if (!subtable.ReadU16(&lookups[i])) {
      return OTS_FAILURE();
    }
    if (lookups[i] < lookup_end || lookups[i] >= length) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < *num_lookups; ++i) {
    if (!ParseLookupTable(file, data + lookups[i], length - lookups[i],
                          num_types, parsers)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseClassDefTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE();
  }
  if (format == 1) {
    return ParseClassDefFormat1(data, length, num_glyphs, num_classes);
  } else if (format == 2) {
    return ParseClassDefFormat2(data, length, num_glyphs, num_classes);
  }

  return OTS_FAILURE();
}

bool ParseCoverageTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE();
  }
  if (format == 1) {
    return ParseCoverageFormat1(data, length, num_glyphs);
  } else if (format == 2) {
    return ParseCoverageFormat2(data, length, num_glyphs);
  }

  return OTS_FAILURE();
}

bool ParseDeviceTable(const uint8_t *data, size_t length) {
  Buffer subtable(data, length);

  uint16_t start_size = 0;
  uint16_t end_size = 0;
  uint16_t delta_format = 0;
  if (!subtable.ReadU16(&start_size) ||
      !subtable.ReadU16(&end_size) ||
      !subtable.ReadU16(&delta_format)) {
    return OTS_FAILURE();
  }
  if (start_size > end_size) {
    OTS_WARNING("bad size range: %u > %u", start_size, end_size);
    return OTS_FAILURE();
  }
  if (delta_format == 0 || delta_format > kMaxDeltaFormatType) {
    OTS_WARNING("bad delta format: %u", delta_format);
    return OTS_FAILURE();
  }
  // The number of delta values per uint16. The device table should contain
  // at least |num_units| * 2 bytes compressed data.
  const unsigned num_units = (end_size - start_size) /
      (1 << (4 - delta_format)) + 1;
  // Just skip |num_units| * 2 bytes since the compressed data could take
  // arbitrary values.
  if (!subtable.Skip(num_units * 2)) {
    return OTS_FAILURE();
  }
  return true;
}

}  // namespace ots

