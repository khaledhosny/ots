// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hhea.h"

#include "head.h"
#include "maxp.h"

// hhea - Horizontal Header
// http://www.microsoft.com/typography/otspec/hhea.htm

namespace ots {

bool OpenTypeHHEA::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!table.ReadU32(&this->version)) {
    return Error("Failed to read table version");
  }
  if (this->version >> 16 != 1) {
    return Error("Bad table version of %d", this->version);
  }

  return OpenTypeMetricsHeader::Parse(data, length);
}

bool OpenTypeHHEA::Serialize(OTSStream *out) {
  return OpenTypeMetricsHeader::Serialize(out);
}

bool ots_hhea_parse(Font *font, const uint8_t *data, size_t length) {
  font->hhea = new OpenTypeHHEA(font);
  return font->hhea->Parse(data, length);
}

bool ots_hhea_should_serialise(Font *font) {
  return font->hhea != NULL && font->hhea->ShouldSerialize();
}

bool ots_hhea_serialise(OTSStream *out, Font *font) {
  return font->hhea->Serialize(out);
}

void ots_hhea_reuse(Font *font, Font *other) {
  font->hhea = other->hhea;
  font->hhea_reused = true;
}

void ots_hhea_free(Font *font) {
  delete font->hhea;
}

}  // namespace ots
