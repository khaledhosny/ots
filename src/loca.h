// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_LOCA_H_
#define OTS_LOCA_H_

#include <vector>

#include "ots.h"

namespace ots {

class OpenTypeLOCA : public Table {
 public:
  explicit OpenTypeLOCA(Font *font)
      : Table(font, OTS_TAG_LOCA) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  std::vector<uint32_t> offsets;
};

}  // namespace ots

#endif  // OTS_LOCA_H_
