// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CFF_H_
#define OTS_CFF_H_

#include "ots.h"

#include <string>

namespace ots {

struct OpenTypeCFF {
  const uint8_t *data;
  size_t length;
  // Name INDEX. This name is used in name.cc as a postscript font name.
  std::string name;
};

}  // namespace ots

#endif  // OTS_CFF_H_
