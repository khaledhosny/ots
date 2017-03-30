// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GLYF_H_
#define OTS_GLYF_H_

#include <new>
#include <utility>
#include <vector>

#include "ots.h"

namespace ots {
class OpenTypeMAXP;

class OpenTypeGLYF : public Table {
 public:
  explicit OpenTypeGLYF(Font *font, uint32_t tag)
      : Table(font, tag), maxp(NULL) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

 private:
  bool ParseFlagsForSimpleGlyph(ots::Buffer *table,
                                uint32_t gly_length,
                                uint32_t num_flags,
                                uint32_t *flags_count_logical,
                                uint32_t *flags_count_physical,
                                uint32_t *xy_coordinates_length);
  bool ParseSimpleGlyph(const uint8_t *data,
                        ots::Buffer *table,
                        int16_t num_contours,
                        uint32_t gly_offset,
                        uint32_t gly_length,
                        uint32_t *new_size);
  bool ParseCompositeGlyph(const uint8_t *data,
                           uint32_t glyph_offset,
                           uint32_t glyph_length,
                           uint32_t *new_size);

  OpenTypeMAXP* maxp;

  std::vector<std::pair<const uint8_t*, size_t> > iov;
};

}  // namespace ots

#endif  // OTS_GLYF_H_
