// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CMAP_H_
#define OTS_CMAP_H_

#include <vector>

#include "ots.h"

namespace ots {

struct OpenTypeCMAPSubtableRange {
  uint32_t start_range;
  uint32_t end_range;
  uint32_t start_glyph_id;
};

struct OpenTypeCMAP {
  OpenTypeCMAP()
      : subtable_304_data(NULL),
        subtable_304_length(0),
        subtable_314_data(NULL),
        subtable_314_length(0) {
  }

  const uint8_t *subtable_304_data;
  size_t subtable_304_length;
  const uint8_t *subtable_314_data;
  size_t subtable_314_length;
  std::vector<OpenTypeCMAPSubtableRange> subtable_31012;
  std::vector<OpenTypeCMAPSubtableRange> subtable_31013;
  std::vector<uint8_t> subtable_100;
};

}  // namespace ots

#endif
