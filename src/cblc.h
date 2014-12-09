// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CBLC_H_
#define OTS_CBLC_H_

#include "ots.h"

namespace ots {

struct OpenTypeCBLC {
  OpenTypeCBLC()
      : data(NULL),
        length(0) {
  }

  const uint8_t *data;
  size_t length;
};

}  // namespace ots

#endif  // OTS_CBLC_H_
