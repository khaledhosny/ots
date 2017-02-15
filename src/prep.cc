// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "prep.h"

// prep - Control Value Program
// http://www.microsoft.com/typography/otspec/prep.htm

namespace ots {

bool OpenTypePREP::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (length >= 128 * 1024u) {
    return Error("table length %ld > 120K", length);  // almost all prep tables are less than 9k bytes.
  }

  if (!table.Skip(length)) {
    return Error("Failed to read table of length %ld", length);
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypePREP::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write table length");
  }

  return true;
}

bool OpenTypePREP::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         GetFont()->glyf != NULL; // this table is not for CFF fonts.
}

bool ots_prep_parse(Font *font, const uint8_t *data, size_t length) {
  font->prep = new OpenTypePREP(font);
  return font->prep->Parse(data, length);
}

bool ots_prep_should_serialise(Font *font) {
  return font->prep != NULL && font->prep->ShouldSerialize();
}

bool ots_prep_serialise(OTSStream *out, Font *font) {
  return font->prep->Serialize(out);
}

void ots_prep_reuse(Font *font, Font *other) {
  font->prep = other->prep;
  font->prep_reused = true;
}

void ots_prep_free(Font *font) {
  delete font->prep;
}

}  // namespace ots
