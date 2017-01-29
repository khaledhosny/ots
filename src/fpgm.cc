// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fpgm.h"

// fpgm - Font Program
// http://www.microsoft.com/typography/otspec/fpgm.htm

namespace ots {

bool OpenTypeFPGM::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (length >= 128 * 1024u) {
    return Error("length (%ld) > 120", length);  // almost all fpgm tables are less than 5k bytes.
  }

  if (!table.Skip(length)) {
    return Error("Bad fpgm length");
  }

  this->data = data;
  this->length = length;
  return true;
}

bool OpenTypeFPGM::Serialize(OTSStream *out) {
  if (!out->Write(this->data, this->length)) {
    return Error("Failed to write fpgm table");
  }

  return true;
}

bool OpenTypeFPGM::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         GetFont()->glyf != NULL; // this table is not for CFF fonts.
}

bool ots_fpgm_parse(Font *font, const uint8_t *data, size_t length) {
  font->fpgm = new OpenTypeFPGM(font);
  return font->fpgm->Parse(data, length);
}

bool ots_fpgm_should_serialise(Font *font) {
  return font->fpgm != NULL && font->fpgm->ShouldSerialize();
}

bool ots_fpgm_serialise(OTSStream *out, Font *font) {
  return font->fpgm->Serialize(out);
}

void ots_fpgm_reuse(Font *font, Font *other) {
  font->fpgm = other->fpgm;
  font->fpgm_reused = true;
}

void ots_fpgm_free(Font *font) {
  delete font->fpgm;
}

}  // namespace ots
