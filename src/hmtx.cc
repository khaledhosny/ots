// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hmtx.h"

#include "hhea.h"
#include "maxp.h"

// hmtx - Horizontal Metrics
// http://www.microsoft.com/typography/otspec/hmtx.htm

namespace ots {

bool OpenTypeHMTX::Parse(const uint8_t *data, size_t length) {
  if (!GetFont()->hhea || !GetFont()->maxp) {
    return Error("Missing hhea or maxp tables in font, needed by hmtx");
  }

  return OpenTypeMetricsTable::Parse(data, length);
}

bool OpenTypeHMTX::Serialize(OTSStream *out) {
  return OpenTypeMetricsTable::Serialize(out);
}

bool ots_hmtx_parse(Font *font, const uint8_t *data, size_t length) {
  font->hmtx = new OpenTypeHMTX(font);
  return font->hmtx->Parse(data, length);
}

bool ots_hmtx_should_serialise(Font *font) {
  return font->hmtx != NULL && font->hmtx->ShouldSerialize();
}

bool ots_hmtx_serialise(OTSStream *out, Font *font) {
  return font->hmtx->Serialize(out);
}

void ots_hmtx_reuse(Font *font, Font *other) {
  font->hmtx = other->hmtx;
  font->hmtx_reused = true;
}

void ots_hmtx_free(Font *font) {
  delete font->hmtx;
}

}  // namespace ots
