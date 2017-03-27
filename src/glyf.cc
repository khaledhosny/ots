// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "glyf.h"

#include <algorithm>
#include <limits>

#include "head.h"
#include "loca.h"
#include "maxp.h"

// glyf - Glyph Data
// http://www.microsoft.com/typography/otspec/glyf.htm

namespace ots {

bool OpenTypeGLYF::ParseFlagsForSimpleGlyph(ots::Buffer *table,
                                            uint32_t gly_length,
                                            uint32_t num_flags,
                                            uint32_t *flags_count_logical,
                                            uint32_t *flags_count_physical,
                                            uint32_t *xy_coordinates_length) {
  uint8_t flag = 0;
  if (!table->ReadU8(&flag)) {
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

  if (flag & (1u << 3)) {  // repeat
    if (*flags_count_logical + 1 >= num_flags) {
      return Error("Count too high (%d + 1 >= %d)", *flags_count_logical, num_flags);
    }
    uint8_t repeat = 0;
    if (!table->ReadU8(&repeat)) {
      return Error("Can't read repeat value");
    }
    if (repeat == 0) {
      return Error("Zero repeat");
    }
    delta += (delta * repeat);

    *flags_count_logical += repeat;
    if (*flags_count_logical >= num_flags) {
      return Error("Count too high (%d >= %d)", *flags_count_logical, num_flags);
    }
    ++(*flags_count_physical);
  }

  if ((flag & (1u << 6)) || (flag & (1u << 7))) {  // reserved flags
    return Error("Bad glyph flag value (%d), reserved flags must be set to zero", flag);
  }

  *xy_coordinates_length += delta;
  if (gly_length < *xy_coordinates_length) {
    return Error("Glyph coordinates length too low (%d < %d)", gly_length, *xy_coordinates_length);
  }

  return true;
}

bool OpenTypeGLYF::ParseSimpleGlyph(const uint8_t *data,
                                    ots::Buffer *table,
                                    int16_t num_contours,
                                    uint32_t gly_offset,
                                    uint32_t gly_length,
                                    uint32_t *new_size) {
  // read the end-points array
  uint16_t num_flags = 0;
  for (int i = 0; i < num_contours; ++i) {
    uint16_t tmp_index = 0;
    if (!table->ReadU16(&tmp_index)) {
      return Error("Can't read contour index %d", i);
    }
    if (tmp_index == 0xffffu) {
      return Error("Bad contour index %d", i);
    }
    // check if the indices are monotonically increasing
    if (i && (tmp_index + 1 <= num_flags)) {
      return Error("Decreasing contour index %d + 1 <= %d", tmp_index, num_flags);
    }
    num_flags = tmp_index + 1;
  }

  uint16_t bytecode_length = 0;
  if (!table->ReadU16(&bytecode_length)) {
    return Error("Can't read bytecode length");
  }

  OpenTypeMAXP *maxp = dynamic_cast<OpenTypeMAXP*>(
      GetFont()->GetTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table missing");
  }
  if (maxp->version_1 &&
      (maxp->max_size_glyf_instructions < bytecode_length)) {
    return Error("Bytecode length too high %d", bytecode_length);
  }

  const uint32_t gly_header_length = 10 + num_contours * 2 + 2;
  if (gly_length < (gly_header_length + bytecode_length)) {
    return Error("Glyph header length too high %d", gly_header_length);
  }

  this->iov.push_back(std::make_pair(
      data + gly_offset,
      static_cast<size_t>(gly_header_length + bytecode_length)));

  if (!table->Skip(bytecode_length)) {
    return Error("Can't skip bytecode of length %d", bytecode_length);
  }

  uint32_t flags_count_physical = 0;  // on memory
  uint32_t xy_coordinates_length = 0;
  for (uint32_t flags_count_logical = 0;
       flags_count_logical < num_flags;
       ++flags_count_logical, ++flags_count_physical) {
    if (!ParseFlagsForSimpleGlyph(table,
                                  gly_length,
                                  num_flags,
                                  &flags_count_logical,
                                  &flags_count_physical,
                                  &xy_coordinates_length)) {
      return Error("Failed to parse glyph flags %d", flags_count_logical);
    }
  }

  if (gly_length < (gly_header_length + bytecode_length +
                    flags_count_physical + xy_coordinates_length)) {
    return Error("Glyph too short %d", gly_length);
  }

  if (gly_length - (gly_header_length + bytecode_length +
                    flags_count_physical + xy_coordinates_length) > 3) {
    // We allow 0-3 bytes difference since gly_length is 4-bytes aligned,
    // zero-padded length.
    return Error("Invalid glyph length %d", gly_length);
  }

  this->iov.push_back(std::make_pair(
      data + gly_offset + gly_header_length + bytecode_length,
      static_cast<size_t>(flags_count_physical + xy_coordinates_length)));

  *new_size
      = gly_header_length + flags_count_physical + xy_coordinates_length + bytecode_length;

  return true;
}

bool OpenTypeGLYF::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeMAXP *maxp = dynamic_cast<OpenTypeMAXP*>(
      GetFont()->GetTable(OTS_TAG_MAXP));
  OpenTypeLOCA *loca = dynamic_cast<OpenTypeLOCA*>(
      GetFont()->GetTable(OTS_TAG_LOCA));
  OpenTypeHEAD *head = dynamic_cast<OpenTypeHEAD*>(
      GetFont()->GetTable(OTS_TAG_HEAD));
  if (!maxp || !loca || !head) {
    return Error("Missing maxp or loca or head table needed by glyf table");
  }

  const unsigned num_glyphs = maxp->num_glyphs;
  std::vector<uint32_t> &offsets = loca->offsets;

  if (offsets.size() != num_glyphs + 1) {
    return Error("Invalide glyph offsets size %ld != %d", offsets.size(), num_glyphs + 1);
  }

  std::vector<uint32_t> resulting_offsets(num_glyphs + 1);
  uint32_t current_offset = 0;

  for (unsigned i = 0; i < num_glyphs; ++i) {
    const unsigned gly_offset = offsets[i];
    // The LOCA parser checks that these values are monotonic
    const unsigned gly_length = offsets[i + 1] - offsets[i];
    if (!gly_length) {
      // this glyph has no outline (e.g. the space charactor)
      resulting_offsets[i] = current_offset;
      continue;
    }

    if (gly_offset >= length) {
      return Error("Glyph %d offset %d too high %ld", i, gly_offset, length);
    }
    // Since these are unsigned types, the compiler is not allowed to assume
    // that they never overflow.
    if (gly_offset + gly_length < gly_offset) {
      return Error("Glyph %d length (%d < 0)!", i, gly_length);
    }
    if (gly_offset + gly_length > length) {
      return Error("Glyph %d length %d too high", i, gly_length);
    }

    table.set_offset(gly_offset);
    int16_t num_contours, xmin, ymin, xmax, ymax;
    if (!table.ReadS16(&num_contours) ||
        !table.ReadS16(&xmin) ||
        !table.ReadS16(&ymin) ||
        !table.ReadS16(&xmax) ||
        !table.ReadS16(&ymax)) {
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

    unsigned new_size = 0;
    if (num_contours == 0) {
      // This is an empty glyph and shouldn’t have any glyph data, but if it
      // does we will simply ignore it.
      new_size = 0;
    } else if (num_contours > 0) {
      // this is a simple glyph and might contain bytecode
      if (!ParseSimpleGlyph(data, &table,
                            num_contours, gly_offset, gly_length, &new_size)) {
        return Error("Failed to parse glyph %d", i);
      }
    } else {
      // it's a composite glyph without any bytecode. Enqueue the whole thing
      this->iov.push_back(std::make_pair(data + gly_offset,
                                         static_cast<size_t>(gly_length)));
      new_size = gly_length;
    }

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

bool OpenTypeGLYF::Serialize(OTSStream *out) {
  for (unsigned i = 0; i < this->iov.size(); ++i) {
    if (!out->Write(this->iov[i].first, this->iov[i].second)) {
      return Error("Falied to write glyph %d", i);
    }
  }

  return true;
}

}  // namespace ots
