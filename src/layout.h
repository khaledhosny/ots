// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_LAYOUT_H_
#define OTS_LAYOUT_H_

#include "ots.h"

// Utility functions for OpenType layout common table formats.
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace ots {

struct LookupTypeParser {
  uint16_t type;
  bool (*parse)(const OpenTypeFile *file, const uint8_t *data,
                const size_t length);
};

bool ParseScriptListTable(const uint8_t *data, const size_t length,
                          const uint16_t num_features);

bool ParseFeatureListTable(const uint8_t *data, const size_t length,
                           const uint16_t num_lookups,
                           uint16_t* num_features);

bool ParseLookupListTable(OpenTypeFile *file, const uint8_t *data,
                          const size_t length, const size_t num_types,
                          const LookupTypeParser* parsers,
                          uint16_t *num_lookups);

bool ParseClassDefTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes);

bool ParseCoverageTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs);

bool ParseDeviceTable(const uint8_t *data, size_t length);

}  // namespace ots

#endif  // OTS_LAYOUT_H_

