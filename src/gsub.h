// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GSUB_H_
#define OTS_GSUB_H_

#include "ots.h"
#include "layout.h"

namespace ots {

class OpenTypeGSUB : public OpenTypeLayoutTable {
 public:
  explicit OpenTypeGSUB(Font *font, uint32_t tag)
      : OpenTypeLayoutTable(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
};

}  // namespace ots

#endif  // OTS_GSUB_H_

