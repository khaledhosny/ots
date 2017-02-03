// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_HHEA_H_
#define OTS_HHEA_H_

#include "metrics.h"
#include "ots.h"

namespace ots {

class OpenTypeHHEA : public OpenTypeMetricsHeader {
 public:
  explicit OpenTypeHHEA(Font *font)
      : OpenTypeMetricsHeader(font, OTS_TAG('h','h','e','a')) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
};

}  // namespace ots

#endif  // OTS_HHEA_H_
