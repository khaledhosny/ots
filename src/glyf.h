// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GLYF_H_
#define OTS_GLYF_H_

#include <algorithm>
#include <limits>
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
  // Bounding box in font units, empty by default.
  struct Bounds {
    int16_t xmin = std::numeric_limits<int16_t>::max();
    int16_t ymin = std::numeric_limits<int16_t>::max();
    int16_t xmax = std::numeric_limits<int16_t>::min();
    int16_t ymax = std::numeric_limits<int16_t>::min();

    bool empty() const { return xmin > xmax; }
    void Add(int16_t x, int16_t y) {
      xmin = std::min(xmin, x);
      ymin = std::min(ymin, y);
      xmax = std::max(xmax, x);
      ymax = std::max(ymax, y);
    }
    void Add(const Bounds& other) {
      if (!other.empty()) {
        Add(other.xmin, other.ymin);
        Add(other.xmax, other.ymax);
      }
    }
    bool Contains(const Bounds& other) const {
      return other.empty() ||
             (xmin <= other.xmin && ymin <= other.ymin &&
              xmax >= other.xmax && ymax >= other.ymax);
    }
  };

  // What a glyph contributes when it is used as a component of a composite
  // glyph. Computed at most once per glyph, so that shared and repeated
  // components are not expanded again for every path that reaches them.
  struct ComponentInfo {
    enum State : uint8_t { kUnvisited, kInProgress, kDone };
    State state = kUnvisited;
    bool empty = true;        // no glyph data, or numberOfContours == 0
    bool unknown_bounds = false;
    uint32_t points = 0;      // number of points after flattening components
    uint32_t components = 0;  // number of components after flattening
    uint32_t depth = 0;       // levels of nested components below this glyph
    // Conservative bounds of the control points, unless unknown_bounds.
    Bounds bounds;
  };

  struct Component {
    uint16_t flags;
    uint16_t gid;
    int16_t dx = 0, dy = 0;  // or point numbers, without ARGS_ARE_XY_VALUES
    // 2x2 transform in F2Dot14 (x' = xx * x + xy * y, y' = yx * x + yy * y).
    int16_t xx = 1 << 14, yx = 0, xy = 0, yy = 1 << 14;
  };

  bool ParseFlagsForSimpleGlyph(Buffer &glyph,
                                uint32_t num_flags,
                                std::vector<uint8_t>& flags,
                                uint32_t *flag_index,
                                uint32_t *coordinates_length);
  bool ParseSimpleGlyph(Buffer &glyph,
                        unsigned gid,
                        int16_t num_contours,
                        const Bounds& bbox,
                        bool is_tricky_font,
                        ComponentInfo* info);

  // Returns a copy of the first |length| bytes of glyph data, with the bbox
  // of its header, |bbox|, grown to include |bounds|, and shifts the hmtx lsb
  // to preserve pp1.x. Returns NULL if the lsb can't be shifted.
  const uint8_t* FixGlyphHeader(unsigned gid,
                                const uint8_t* data,
                                size_t length,
                                const Bounds& bbox,
                                const Bounds& bounds);

  bool ReadComponent(Buffer& glyph, Component* component);

  // Adds to |bounds| the bounds of a component glyph, |component_bounds|, as
  // placed by |component|. Returns false if they can't be computed.
  static bool AddComponentBounds(const Component& component,
                                 const Bounds& component_bounds,
                                 Bounds* bounds);

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
