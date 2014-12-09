// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cblc.h"

// CBLC
// https://color-emoji.googlecode.com/git/specification/v1.html
// We don't support the table, but provide a way not to drop the table.

namespace ots {

extern bool g_drop_color_bitmap_tables;

bool ots_cblc_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  if (g_drop_color_bitmap_tables) {
    return OTS_FAILURE();
  }

  file->cblc = new OpenTypeCBLC;
  file->cblc->data = data;
  file->cblc->length = length;
  return true;
}

bool ots_cblc_should_serialise(OpenTypeFile *file) {
  return file->cblc != NULL && file->cbdt != NULL;
}

bool ots_cblc_serialise(OTSStream *out, OpenTypeFile *file) {
  if (!out->Write(file->cblc->data, file->cblc->length)) {
    return OTS_FAILURE();
  }
  return true;
}

void ots_cblc_free(OpenTypeFile *file) {
  delete file->cblc;
}

}  // namespace ots
