// Copyright (c) 2011-2024 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_EBSC_H_
#define OTS_EBSC_H_

#include "ots.h"

namespace ots {

class OpenTypeEBSC : public Table {
public:
  explicit OpenTypeEBSC(Font *font, uint32_t tag)
    : Table(font, tag, tag),
    m_data(NULL),
    m_length(0) {
  }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  //private:
  const uint8_t *m_data;
  size_t m_length;
};

}  // namespace ots

#endif  // OTS_EBSC_H_