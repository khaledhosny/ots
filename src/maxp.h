// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_MAXP_H_
#define OTS_MAXP_H_

#include "ots.h"

namespace ots {

class OpenTypeMAXP : public Table {
 public:
  explicit OpenTypeMAXP(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  uint16_t num_glyphs = 0;
  bool version_1 = 0;

  uint16_t max_points = 0;
  uint16_t max_contours = 0;
  uint16_t max_c_points = 0;
  uint16_t max_c_contours = 0;

  uint16_t max_zones = 0;
  uint16_t max_t_points = 0;
  uint16_t max_storage = 0;
  uint16_t max_fdefs = 0;
  uint16_t max_idefs = 0;
  uint16_t max_stack = 0;
  uint16_t max_size_glyf_instructions = 0;

  uint16_t max_c_components = 0;
  uint16_t max_c_depth = 0;
};

}  // namespace ots

#endif  // OTS_MAXP_H_
