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
  // What a glyph contributes when it is used as a component of a composite
  // glyph. Computed at most once per glyph, so that shared and repeated
  // components are not expanded again for every path that reaches them.
  struct ComponentInfo {
    enum State : uint8_t { kUnvisited, kInProgress, kDone };
    State state = kUnvisited;
    bool empty = true;        // no glyph data, or numberOfContours == 0
    uint32_t points = 0;      // number of points after flattening components
    uint32_t components = 0;  // number of components after flattening
    uint32_t depth = 0;       // levels of nested components below this glyph
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
      unsigned* skip_count);

  // Fills in component_info for glyph_id and every glyph it refers to,
  // directly or through nested components.
  bool ComputeComponentInfo(
      const uint8_t *data,
      size_t length,
      const std::vector<uint32_t>& loca_offsets,
      uint16_t glyph_id,
      std::vector<ComponentInfo>* component_info);

  // Reads the point count of a simple glyph into info, or the component
  // glyph ids of a composite glyph into components.
  bool ReadComponentInfo(
      Buffer& glyph,
      ComponentInfo* info,
      std::vector<uint16_t>* components);

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
