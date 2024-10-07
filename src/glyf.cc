// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "glyf.h"

#include <algorithm>
#include <limits>

#include "head.h"
#include "loca.h"
#include "maxp.h"
#include "name.h"

// glyf - Glyph Data
// http://www.microsoft.com/typography/otspec/glyf.htm

#define TABLE_NAME "glyf"

namespace ots {

bool OpenTypeGLYF::ParseFlagsForSimpleGlyph(Buffer &glyph,
                                            uint32_t num_flags,
                                            std::vector<uint8_t>& flags,
                                            uint32_t *flag_index,
                                            uint32_t *coordinates_length) {
  uint8_t flag = 0;
  if (!glyph.ReadU8(&flag)) {
    return Error("Can't read flag");
  }

  uint32_t delta = 0;
  if (flag & (1u << 1)) {  // x-Short
    ++delta;
  } else if (!(flag & (1u << 4))) {
    delta += 2;
  }

  if (flag & (1u << 2)) {  // y-Short
    ++delta;
  } else if (!(flag & (1u << 5))) {
    delta += 2;
  }

  /* MS and Apple specs say this bit is reserved and must be set to zero, but
   * Apple spec then contradicts itself and says it should be set on the first
   * contour flag for simple glyphs with overlapping contours:
   * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6AATIntro.html
   * (“Overlapping contours” section) */
  if (flag & (1u << 6) && *flag_index != 0) {
    return Error("Bad glyph flag (%d), "
                 "bit 6 must be set to zero for flag %d", flag, *flag_index);
  }

  flags[*flag_index] = flag & ~(1u << 3);

  if (flag & (1u << 3)) {  // repeat
    if (*flag_index + 1 >= num_flags) {
      return Error("Count too high (%d + 1 >= %d)", *flag_index, num_flags);
    }
    uint8_t repeat = 0;
    if (!glyph.ReadU8(&repeat)) {
      return Error("Can't read repeat value");
    }
    if (repeat == 0) {
      return Error("Zero repeat");
    }
    delta += (delta * repeat);

    if (*flag_index + repeat >= num_flags) {
      return Error("Count too high (%d >= %d)", *flag_index + repeat, num_flags);
    }

    while (repeat--) {
      flags[++*flag_index] = flag & ~(1u << 3);
    }
  }

  if (flag & (1u << 7)) {  // reserved flag
    return Error("Bad glyph flag (%d), reserved bit 7 must be set to zero", flag);
  }

  *coordinates_length += delta;
  if (glyph.length() < *coordinates_length) {
    return Error("Glyph coordinates length bigger than glyph length (%d > %d)",
                 *coordinates_length, glyph.length());
  }

  return true;
}

#define X_SHORT_VECTOR                        (1u << 1)
#define Y_SHORT_VECTOR                        (1u << 2)
#define X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR  (1u << 4)
#define Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR  (1u << 5)

bool OpenTypeGLYF::ParseSimpleGlyph(Buffer &glyph,
                                    unsigned gid,
                                    int16_t num_contours,
                                    int16_t xmin,
                                    int16_t ymin,
                                    int16_t xmax,
                                    int16_t ymax,
                                    bool is_tricky_font) {
  // read the end-points array
  uint16_t num_flags = 0;
  for (int i = 0; i < num_contours; ++i) {
    uint16_t tmp_index = 0;
    if (!glyph.ReadU16(&tmp_index)) {
      return Error("Can't read contour index %d (glyph %u)", i, gid);
    }
    if (tmp_index == 0xffffu) {
      return Error("Bad contour index %d (glyph %u)", i, gid);
    }
    // check if the indices are monotonically increasing
    if (i && (tmp_index + 1 <= num_flags)) {
      return Error("Decreasing contour index %d + 1 <= %d (glyph %u)", tmp_index, num_flags, gid);
    }
    num_flags = tmp_index + 1;
  }

  if (this->maxp->version_1 &&
      num_flags > this->maxp->max_points) {
    Warning("Number of contour points exceeds maxp maxPoints, adjusting limit (glyph %u)", gid);
    this->maxp->max_points = num_flags;
  }

  uint16_t bytecode_length = 0;
  if (!glyph.ReadU16(&bytecode_length)) {
    return Error("Can't read bytecode length");
  }

  if (this->maxp->version_1 &&
      this->maxp->max_size_glyf_instructions < bytecode_length) {
    Warning("Bytecode length is bigger than maxp.maxSizeOfInstructions %d: %d (glyph %u)",
            this->maxp->max_size_glyf_instructions, bytecode_length, gid);
    this->maxp->max_size_glyf_instructions = bytecode_length;
  }

  if (!glyph.Skip(bytecode_length)) {
    return Error("Can't read bytecode of length %d (glyph %u)", bytecode_length, gid);
  }

  uint32_t coordinates_length = 0;
  std::vector<uint8_t> flags(num_flags);
  for (uint32_t i = 0; i < num_flags; ++i) {
    if (!ParseFlagsForSimpleGlyph(glyph, num_flags, flags, &i, &coordinates_length)) {
      return Error("Failed to parse glyph flags %d (glyph %u)", i, gid);
    }
  }

  bool adjusted_bbox = false;
  int16_t x = 0, y = 0;

  // Read and check x-coords
  for (uint32_t i = 0; i < num_flags; ++i) {
    uint8_t flag = flags[i];
    if (flag & X_SHORT_VECTOR) {
      uint8_t dx;
      if (!glyph.ReadU8(&dx)) {
        return Error("Glyph too short %d (glyph %u)", glyph.length(), gid);
      }
      if (flag & X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) {
        x += dx;
      } else {
        x -= dx;
      }
    } else if (flag & X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) {
      // x remains unchanged
    } else {
      int16_t dx;
      if (!glyph.ReadS16(&dx)) {
        return Error("Glyph too short %d (glyph %u)", glyph.length(), gid);
      }
      x += dx;
    }
    if (x < xmin) {
      xmin = x;
      adjusted_bbox = true;
    }
    if (x > xmax) {
      xmax = x;
      adjusted_bbox = true;
    }
  }

  // Read and check y-coords
  for (uint32_t i = 0; i < num_flags; ++i) {
    uint8_t flag = flags[i];
    if (flag & Y_SHORT_VECTOR) {
      uint8_t dy;
      if (!glyph.ReadU8(&dy)) {
        return Error("Glyph too short %d (glyph %u)", glyph.length(), gid);
      }
      if (flag & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) {
        y += dy;
      } else {
        y -= dy;
      }
    } else if (flag & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) {
      // x remains unchanged
    } else {
      int16_t dy;
      if (!glyph.ReadS16(&dy)) {
        return Error("Glyph too short %d (glyph %u)", glyph.length(), gid);
      }
      y += dy;
    }
    if (y < ymin) {
      ymin = y;
      adjusted_bbox = true;
    }
    if (y > ymax) {
      ymax = y;
      adjusted_bbox = true;
    }
  }

  if (glyph.remaining() > 3) {
    // We allow 0-3 bytes difference since gly_length is 4-bytes aligned,
    // zero-padded length.
    Warning("Extra bytes at end of the glyph: %d (glyph %u)", glyph.remaining(), gid);
  }

  if (adjusted_bbox) {
    if (is_tricky_font) {
      Warning("Glyph bbox was incorrect; NOT adjusting tricky font (glyph %u)", gid);
    } else {
      Warning("Glyph bbox was incorrect; adjusting (glyph %u)", gid);
      // copy the numberOfContours field
      this->iov.push_back(std::make_pair(glyph.buffer(), 2));
      // output a fixed-up version of the bounding box
      uint8_t* fixed_bbox = new uint8_t[8];
      replacements.push_back(fixed_bbox);
      xmin = ots_htons(xmin);
      std::memcpy(fixed_bbox, &xmin, 2);
      ymin = ots_htons(ymin);
      std::memcpy(fixed_bbox + 2, &ymin, 2);
      xmax = ots_htons(xmax);
      std::memcpy(fixed_bbox + 4, &xmax, 2);
      ymax = ots_htons(ymax);
      std::memcpy(fixed_bbox + 6, &ymax, 2);
      this->iov.push_back(std::make_pair(fixed_bbox, 8));
      // copy the remainder of the glyph data
      this->iov.push_back(std::make_pair(glyph.buffer() + 10, glyph.offset() - 10));
      return true;
    }
  }

  this->iov.push_back(std::make_pair(glyph.buffer(), glyph.offset()));

  return true;
}

#define ARG_1_AND_2_ARE_WORDS    (1u << 0)
#define WE_HAVE_A_SCALE          (1u << 3)
#define MORE_COMPONENTS          (1u << 5)
#define WE_HAVE_AN_X_AND_Y_SCALE (1u << 6)
#define WE_HAVE_A_TWO_BY_TWO     (1u << 7)
#define WE_HAVE_INSTRUCTIONS     (1u << 8)

bool OpenTypeGLYF::ParseCompositeGlyph(
    Buffer &glyph,
    unsigned glyph_id,
    ComponentPointCount* component_point_count,
    unsigned* skip_count) {
  uint16_t flags = 0;
  uint16_t gid = 0;
  enum class edit_t : uint8_t {
    skip_bytes,  // param is number of bytes to skip from offset
    set_flag,    // param is flag to be set (in the 16-bit field at offset)
    clear_flag,  // param is flag to be cleared
  };
  // List of glyph data edits to be applied: first value is offset in the data,
  // second is a pair of <edit-action, param>.
  typedef std::pair<unsigned, std::pair<edit_t, unsigned>> edit_rec;
  std::vector<edit_rec> edits;
  unsigned prev_start = 0;
  bool we_have_instructions = false;
  do {
    unsigned start = glyph.offset();

    if (!glyph.ReadU16(&flags) || !glyph.ReadU16(&gid)) {
      return Error("Can't read composite glyph flags or glyphIndex");
    }

    if (gid >= this->maxp->num_glyphs) {
      return Error("Invalid glyph id used in composite glyph: %d", gid);
    }

    if (flags & ARG_1_AND_2_ARE_WORDS) {
      int16_t argument1;
      int16_t argument2;
      if (!glyph.ReadS16(&argument1) || !glyph.ReadS16(&argument2)) {
        return Error("Can't read argument1 or argument2");
      }
    } else {
      uint8_t argument1;
      uint8_t argument2;
      if (!glyph.ReadU8(&argument1) || !glyph.ReadU8(&argument2)) {
        return Error("Can't read argument1 or argument2");
      }
    }

    if (flags & WE_HAVE_A_SCALE) {
      int16_t scale;
      if (!glyph.ReadS16(&scale)) {
        return Error("Can't read scale");
      }
    } else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
      int16_t xscale;
      int16_t yscale;
      if (!glyph.ReadS16(&xscale) || !glyph.ReadS16(&yscale)) {
        return Error("Can't read xscale or yscale");
      }
    } else if (flags & WE_HAVE_A_TWO_BY_TWO) {
      int16_t xscale;
      int16_t scale01;
      int16_t scale10;
      int16_t yscale;
      if (!glyph.ReadS16(&xscale) ||
          !glyph.ReadS16(&scale01) ||
          !glyph.ReadS16(&scale10) ||
          !glyph.ReadS16(&yscale)) {
        return Error("Can't read transform");
      }
    }

    if (this->loca->offsets[gid] == this->loca->offsets[gid + 1]) {
      Warning("empty gid %u used as component in glyph %u", gid, glyph_id);
      // DirectWrite chokes on composite glyphs that have a completely empty glyph
      // as a component; see https://github.com/mozilla/pdf.js/issues/18848.
      // To work around this, we attempt to drop empty components.
      // But we don't drop the component if it's the only (remaining) one in the composite.
      if (prev_start > 0 || (flags & MORE_COMPONENTS)) {
        if (!(flags & MORE_COMPONENTS)) {
          // We're dropping the last component, so we need to clear the MORE_COMPONENTS flag
          // on the previous one.
          edits.push_back(edit_rec{prev_start, std::make_pair(edit_t::clear_flag, MORE_COMPONENTS)});
        }
        // If this component was the first to provide WE_HAVE_INSTRUCTIONS, set it on the previous (if any).
        if ((flags & WE_HAVE_INSTRUCTIONS) && !we_have_instructions && prev_start > 0) {
          edits.push_back(edit_rec{prev_start, std::make_pair(edit_t::set_flag, WE_HAVE_INSTRUCTIONS)});
        }
        // Finally, skip the actual bytes of this component.
        edits.push_back(edit_rec{start, std::make_pair(edit_t::skip_bytes, glyph.offset() - start)});
      }
    } else {
      // If this is the first component we're keeping, but we already saw WE_HAVE_INSTRUCTIONS
      // (on a dropped component), we need to ensure that flag is set here.
      if (prev_start == 0 && we_have_instructions && !(flags & WE_HAVE_INSTRUCTIONS)) {
        edits.push_back(edit_rec{start, std::make_pair(edit_t::set_flag, WE_HAVE_INSTRUCTIONS)});
      }
      prev_start = start;
    }

    we_have_instructions = we_have_instructions || (flags & WE_HAVE_INSTRUCTIONS);

    // Push initial components on stack at level 1
    // to traverse them in parent function.
    component_point_count->gid_stack.push_back({gid, 1});
  } while (flags & MORE_COMPONENTS);

  // Sort any required edits by offset in the glyph data.
  struct {
    bool operator() (const edit_rec& a, const edit_rec& b) const {
      return a.first < b.first;
    }
  } cmp;
  std::sort(edits.begin(), edits.end(), cmp);

  if (we_have_instructions) {
    uint16_t bytecode_length;
    if (!glyph.ReadU16(&bytecode_length)) {
      return Error("Can't read instructions size");
    }

    if (this->maxp->version_1 &&
        this->maxp->max_size_glyf_instructions < bytecode_length) {
      Warning("Bytecode length is bigger than maxp.maxSizeOfInstructions "
              "%d: %d",
              this->maxp->max_size_glyf_instructions, bytecode_length);
      this->maxp->max_size_glyf_instructions = bytecode_length;
    }

    if (!glyph.Skip(bytecode_length)) {
      return Error("Can't read bytecode of length %d", bytecode_length);
    }
  }

  // Record the glyph data in this->iov, accounting for any required edits.
  *skip_count = 0;
  unsigned offset = 0;
  while (!edits.empty()) {
    auto& edit = edits.front();
    // Handle any glyph data between current offset and the next edit position.
    if (edit.first > offset) {
      this->iov.push_back(std::make_pair(glyph.buffer() + offset, edit.first - offset));
      offset = edit.first;
    }

    // Handle the edit. Note that there may be multiple set_flag/clear_flag edits
    // at the same offset, but skip_bytes will never coincide with another edit.
    auto& action = edit.second;
    switch (action.first) {
      case edit_t::set_flag:
      case edit_t::clear_flag: {
        // Read the existing flags word.
        uint16_t flags;
        std::memcpy(&flags, glyph.buffer() + offset, 2);
        flags = ots_ntohs(flags);
        // Apply all flag changes for the current offset.
        while (!edits.empty() && edits.front().first == offset) {
          auto& e = edits.front();
          switch (e.second.first) {
            case edit_t::set_flag:
              flags |= e.second.second;
              break;
            case edit_t::clear_flag:
              flags &= ~e.second.second;
              break;
            default:
              assert(false);
              break;
          }
          edits.erase(edits.begin());
        }
        // Record the modified flags word.
        flags = ots_htons(flags);
        uint8_t* flags_data = new uint8_t[2];
        std::memcpy(flags_data, &flags, 2);
        replacements.push_back(flags_data);
        this->iov.push_back(std::make_pair(flags_data, 2));
        offset += 2;
        break;
      }

      case edit_t::skip_bytes:
        offset = edit.first + action.second;
        *skip_count += action.second;
        edits.erase(edits.begin());
        break;

      default:
        assert(false);
        break;
    }
  }

  // Handle any remaining glyph data after the last edit.
  if (glyph.offset() > offset) {
    this->iov.push_back(std::make_pair(glyph.buffer() + offset, glyph.offset() - offset));
  }

  return true;
}

bool OpenTypeGLYF::Parse(const uint8_t *data, size_t length) {
  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  OpenTypeLOCA *loca = static_cast<OpenTypeLOCA*>(
      GetFont()->GetTypedTable(OTS_TAG_LOCA));
  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));
  if (!maxp || !loca || !head) {
    return Error("Missing maxp or loca or head table needed by glyf table");
  }

  OpenTypeNAME *name = static_cast<OpenTypeNAME*>(
      GetFont()->GetTypedTable(OTS_TAG_NAME));
  bool is_tricky = name->IsTrickyFont();

  this->loca = loca;
  this->maxp = maxp;

  const unsigned num_glyphs = maxp->num_glyphs;
  std::vector<uint32_t> &offsets = loca->offsets;

  if (offsets.size() != num_glyphs + 1) {
    return Error("Invalid glyph offsets size %ld != %d", offsets.size(), num_glyphs + 1);
  }

  std::vector<uint32_t> resulting_offsets(num_glyphs + 1);
  uint32_t current_offset = 0;

  for (unsigned i = 0; i < num_glyphs; ++i) {
    // Used by ParseCompositeGlyph to return the number of bytes being skipped
    // in the glyph description, so we can adjust offsets properly.
    unsigned skip_count = 0;

    Buffer glyph(GetGlyphBufferSection(data, length, offsets, i));
    if (!glyph.buffer())
      return false;

    if (!glyph.length()) {
      resulting_offsets[i] = current_offset;
      continue;
    }

    int16_t num_contours, xmin, ymin, xmax, ymax;
    if (!glyph.ReadS16(&num_contours) ||
        !glyph.ReadS16(&xmin) ||
        !glyph.ReadS16(&ymin) ||
        !glyph.ReadS16(&xmax) ||
        !glyph.ReadS16(&ymax)) {
      return Error("Can't read glyph %d header", i);
    }

    if (num_contours <= -2) {
      // -2, -3, -4, ... are reserved for future use.
      return Error("Bad number of contours %d in glyph %d", num_contours, i);
    }

    // workaround for fonts in http://www.princexml.com/fonts/
    if ((xmin == 32767) &&
        (xmax == -32767) &&
        (ymin == 32767) &&
        (ymax == -32767)) {
      Warning("bad xmin/xmax/ymin/ymax values");
      xmin = xmax = ymin = ymax = 0;
    }

    if (xmin > xmax || ymin > ymax) {
      return Error("Bad bounding box values bl=(%d, %d), tr=(%d, %d) in glyph %d", xmin, ymin, xmax, ymax, i);
    }

    if (num_contours == 0) {
      // This is an empty glyph and shouldn’t have any glyph data, but if it
      // does we will simply ignore it.
      glyph.set_offset(0);
    } else if (num_contours > 0) {
      if (!ParseSimpleGlyph(glyph, i, num_contours, xmin, ymin, xmax, ymax, is_tricky)) {
        return Error("Failed to parse glyph %d", i);
      }
    } else {

      ComponentPointCount component_point_count;
      if (!ParseCompositeGlyph(glyph, i, &component_point_count, &skip_count)) {
        return Error("Failed to parse glyph %d", i);
      }

      // Check maxComponentDepth and validate maxComponentPoints.
      // ParseCompositeGlyph placed the first set of component glyphs on the
      // component_point_count.gid_stack, which we start to process below. If a
      // nested glyph is in turn a component glyph, additional glyphs are placed
      // on the stack.
      while (component_point_count.gid_stack.size()) {
        GidAtLevel stack_top_gid = component_point_count.gid_stack.back();
        component_point_count.gid_stack.pop_back();

        Buffer points_count_glyph(GetGlyphBufferSection(
            data,
            length,
            offsets,
            stack_top_gid.gid));

        if (!points_count_glyph.buffer())
          return false;

        if (!points_count_glyph.length())
          continue;

        if (!TraverseComponentsCountingPoints(points_count_glyph,
                                              i,
                                              stack_top_gid.level,
                                              &component_point_count)) {
          return Error("Error validating component points and depth.");
        }

        if (component_point_count.accumulated_component_points >
            std::numeric_limits<uint16_t>::max()) {
          return Error("Illegal composite points value "
                       "exceeding 0xFFFF for base glyph %d.", i);
        } else if (this->maxp->version_1 &&
                   component_point_count.accumulated_component_points >
                   this->maxp->max_c_points) {
          Warning("Number of composite points in glyph %d exceeds "
                  "maxp maxCompositePoints: %d vs %d, adjusting limit.",
                  i,
                  component_point_count.accumulated_component_points,
                  this->maxp->max_c_points
                  );
          this->maxp->max_c_points =
              component_point_count.accumulated_component_points;
        }
      }
    }

    size_t new_size = glyph.offset() - skip_count;
    resulting_offsets[i] = current_offset;
    // glyphs must be four byte aligned
    // TODO(yusukes): investigate whether this padding is really necessary.
    //                Which part of the spec requires this?
    const unsigned padding = (4 - (new_size & 3)) % 4;
    if (padding) {
      this->iov.push_back(std::make_pair(
          reinterpret_cast<const uint8_t*>("\x00\x00\x00\x00"),
          static_cast<size_t>(padding)));
      new_size += padding;
    }
    current_offset += new_size;
  }
  resulting_offsets[num_glyphs] = current_offset;

  const uint16_t max16 = std::numeric_limits<uint16_t>::max();
  if ((*std::max_element(resulting_offsets.begin(),
                         resulting_offsets.end()) >= (max16 * 2u)) &&
      (head->index_to_loc_format != 1)) {
    head->index_to_loc_format = 1;
  }

  loca->offsets = resulting_offsets;

  if (this->iov.empty()) {
    // As a special case when all glyph in the font are empty, add a zero byte
    // to the table, so that we don’t reject it down the way, and to make the
    // table work on Windows as well.
    // See https://github.com/khaledhosny/ots/issues/52
    static const uint8_t kZero = 0;
    this->iov.push_back(std::make_pair(&kZero, 1));
  }

  return true;
}

bool OpenTypeGLYF::TraverseComponentsCountingPoints(
    Buffer &glyph,
    uint16_t base_glyph_id,
    uint32_t level,
    ComponentPointCount* component_point_count) {

  int16_t num_contours;
  if (!glyph.ReadS16(&num_contours) ||
      !glyph.Skip(8)) {
    return Error("Can't read glyph header.");
  }

  if (num_contours <= -2) {
    return Error("Bad number of contours %d in glyph.", num_contours);
  }

  if (num_contours == 0)
    return true;

  // FontTools counts a component level for each traversed recursion. We start
  // counting at level 0. If we reach a level that's deeper than
  // maxComponentDepth, we expand maxComponentDepth unless it's larger than
  // the maximum possible depth.
  if (level > std::numeric_limits<uint16_t>::max()) {
    return Error("Illegal component depth exceeding 0xFFFF in base glyph id %d.",
                 base_glyph_id);
  } else if (this->maxp->version_1 &&
             level > this->maxp->max_c_depth) {
    this->maxp->max_c_depth = level;
    Warning("Component depth exceeds maxp maxComponentDepth "
            "in glyph %d, adjust limit to %d.",
            base_glyph_id, level);
  }

  if (num_contours > 0) {
    uint16_t num_points = 0;
    for (int i = 0; i < num_contours; ++i) {
      // Simple glyph, add contour points.
      uint16_t tmp_index = 0;
      if (!glyph.ReadU16(&tmp_index)) {
        return Error("Can't read contour index %d", i);
      }
      num_points = tmp_index + 1;
    }

    component_point_count->accumulated_component_points += num_points;
    return true;
  } else  {
    assert(num_contours == -1);

    // Composite glyph, add gid's to stack.
    uint16_t flags = 0;
    uint16_t gid = 0;
    do {
      if (!glyph.ReadU16(&flags) || !glyph.ReadU16(&gid)) {
        return Error("Can't read composite glyph flags or glyphIndex");
      }

      size_t skip_bytes = 0;
      skip_bytes += flags & ARG_1_AND_2_ARE_WORDS ? 4 : 2;

      if (flags & WE_HAVE_A_SCALE) {
        skip_bytes += 2;
      } else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
        skip_bytes += 4;
      } else if (flags & WE_HAVE_A_TWO_BY_TWO) {
        skip_bytes += 8;
      }

      if (!glyph.Skip(skip_bytes)) {
        return Error("Failed to parse component glyph.");
      }

      if (gid >= this->maxp->num_glyphs) {
        return Error("Invalid glyph id used in composite glyph: %d", gid);
      }

      component_point_count->gid_stack.push_back({gid, level + 1u});
    } while (flags & MORE_COMPONENTS);
    return true;
  }
}

Buffer OpenTypeGLYF::GetGlyphBufferSection(
    const uint8_t *data,
    size_t length,
    const std::vector<uint32_t>& loca_offsets,
    unsigned glyph_id) {

  Buffer null_buffer(nullptr, 0);

  const unsigned gly_offset = loca_offsets[glyph_id];
  // The LOCA parser checks that these values are monotonic
  const unsigned gly_length = loca_offsets[glyph_id + 1] - loca_offsets[glyph_id];
  if (!gly_length) {
    // this glyph has no outline (e.g. the space character)
    return Buffer(data + gly_offset, 0);
  }

  if (gly_offset >= length) {
    Error("Glyph %d offset %d too high %ld", glyph_id, gly_offset, length);
    return null_buffer;
  }
  // Since these are unsigned types, the compiler is not allowed to assume
  // that they never overflow.
  if (gly_offset + gly_length < gly_offset) {
    Error("Glyph %d length (%d < 0)!", glyph_id, gly_length);
    return null_buffer;
  }
  if (gly_offset + gly_length > length) {
    Error("Glyph %d length %d too high", glyph_id, gly_length);
    return null_buffer;
  }

  return Buffer(data + gly_offset, gly_length);
}

bool OpenTypeGLYF::Serialize(OTSStream *out) {
  for (unsigned i = 0; i < this->iov.size(); ++i) {
    if (!out->Write(this->iov[i].first, this->iov[i].second)) {
      return Error("Failed to write glyph %d", i);
    }
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
