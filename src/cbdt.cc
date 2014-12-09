// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cbdt.h"

// CBDT
// https://color-emoji.googlecode.com/git/specification/v1.html
// We don't support the table, but provide a way not to drop the table.

namespace ots {

extern bool g_drop_color_bitmap_tables;

bool ots_cbdt_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  if (g_drop_color_bitmap_tables) {
    return OTS_FAILURE();
  }

  file->cbdt = new OpenTypeCBDT;
  file->cbdt->data = data;
  file->cbdt->length = length;
  return true;
}

bool ots_cbdt_should_serialise(OpenTypeFile *file) {
  return file->cbdt != NULL && file->cblc != NULL;
}

bool ots_cbdt_serialise(OTSStream *out, OpenTypeFile *file) {
  if (!out->Write(file->cbdt->data, file->cbdt->length)) {
    return OTS_FAILURE();
  }
  return true;
}

void ots_cbdt_free(OpenTypeFile *file) {
  delete file->cbdt;
}

}  // namespace ots
