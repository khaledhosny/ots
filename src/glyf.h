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
class OpenTypeLOCA;
class OpenTypeMAXP;

class OpenTypeGLYF : public Table {
 public:
  explicit OpenTypeGLYF(Font *font, uint32_t tag)
      : Table(font, tag, tag), maxp(NULL) { }

  ~OpenTypeGLYF() {
    for (auto* p : replacements) {
      delete[] p;
    }
  }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

 private:
  struct GidAtLevel {
    uint16_t gid;
    uint32_t level;
  };

  struct ComponentPointCount {
    ComponentPointCount() : accumulated_component_points(0) {};
    uint32_t accumulated_component_points;
    std::vector<GidAtLevel> gid_stack;
  };

  bool ParseFlagsForSimpleGlyph(Buffer &glyph,
                                uint32_t num_flags,
                                std::vector<uint8_t>& flags,
                                uint32_t *flag_index,
                                uint32_t *coordinates_length);
  bool ParseSimpleGlyph(Buffer &glyph,
                        unsigned gid,
                        int16_t num_contours,
                        int16_t xmin,
                        int16_t ymin,
                        int16_t xmax,
                        int16_t ymax,
                        bool is_tricky_font);

  // The skip_count outparam returns the number of bytes from the original
  // glyph description that are being skipped on output (normally zero).
  bool ParseCompositeGlyph(
      Buffer &glyph,
      unsigned glyph_id,
      ComponentPointCount* component_point_count,
      unsigned* skip_count);

  bool TraverseComponentsCountingPoints(
      Buffer& glyph,
      uint16_t base_glyph_id,
      uint32_t level,
      ComponentPointCount* component_point_count);

  Buffer GetGlyphBufferSection(
      const uint8_t *data,
      size_t length,
      const std::vector<uint32_t>& loca_offsets,
      unsigned glyph_id);

  OpenTypeLOCA* loca;
  OpenTypeMAXP* maxp;

  std::vector<std::pair<const uint8_t*, size_t> > iov;

  // Any blocks of replacement data created during parsing are stored here
  // to be available during serialization.
  std::vector<uint8_t*> replacements;
};

}  // namespace ots

#endif  // OTS_GLYF_H_
