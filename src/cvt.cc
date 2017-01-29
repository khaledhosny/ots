// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cvt.h"

// cvt - Control Value Table
// http://www.microsoft.com/typography/otspec/cvt.htm

namespace ots {

bool OpenTypeCVT::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (length >= 128 * 1024u) {
    return Error("Length (%d) > 120K");  // almost all cvt tables are less than 4k bytes.
  }

  if (length % 2 != 0) {
    return Error("Uneven cvt length (%d)", length);
  }

  if (!table.Skip(length)) {
    return Error("Length too high");
  }

  this->data = data;
  this->length = length;
  return true;
}

bool OpenTypeCVT::Serialize(OTSStream *out) {
  if (!out->Write(this->data, this->length)) {
    return Error("Failed to write cvt table");
  }

  return true;
}

bool OpenTypeCVT::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         GetFont()->glyf != NULL; // this table is not for CFF fonts.
}

bool ots_cvt_parse(Font *font, const uint8_t *data, size_t length) {
  font->cvt = new OpenTypeCVT(font);
  return font->cvt->Parse(data, length);
}

bool ots_cvt_should_serialise(Font *font) {
  return font->cvt != NULL && font->cvt->ShouldSerialize();
}

bool ots_cvt_serialise(OTSStream *out, Font *font) {
  return font->cvt->Serialize(out);
}

void ots_cvt_reuse(Font *font, Font *other) {
  font->cvt = other->cvt;
  font->cvt_reused = true;
}

void ots_cvt_free(Font *font) {
  delete font->cvt;
}

}  // namespace ots
