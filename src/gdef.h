// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GDEF_H_
#define OTS_GDEF_H_

#include "ots.h"

namespace ots {

struct OpenTypeGDEF {
  OpenTypeGDEF()
      : version_2(false),
        data(NULL),
        length(0) {
  }

  bool version_2;
  const uint8_t *data;
  size_t length;
};

}  // namespace ots

#endif

