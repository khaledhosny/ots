// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout.h"

// OpenType Layout Common Table Formats
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace {

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
  if (range_count >= num_glyphs) {
    OTS_WARNING("bad range count: %u", range_count);
    return OTS_FAILURE();
  }
  uint16_t last_end = 0;
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
  }

  return true;
}

}  // namespace

namespace ots {

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

}  // namespace ots

