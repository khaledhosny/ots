// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_MEMORY_STREAM_H_
#define OTS_MEMORY_STREAM_H_

#include <cstring>
#include <limits>

#include "opentype-sanitiser.h"

namespace ots {

class MemoryStream : public OTSStream {
 public:
  MemoryStream(void *ptr, size_t length)
      : ptr_(ptr), length_(length), off_(0) {
  }

  bool WriteRaw(const void *data, size_t length) {
    if ((off_ + length > length_) ||
        (length > std::numeric_limits<size_t>::max() - off_)) {
      return false;
    }
    std::memcpy(static_cast<char*>(ptr_) + off_, data, length);
    off_ += length;
    return true;
  }

  bool Seek(off_t position) {
    if (position < 0) return false;
    if (static_cast<size_t>(position) > length_) return false;
    off_ = position;
    return true;
  }

  off_t Tell() const {
    return off_;
  }

 private:
  void * const ptr_;
  size_t length_;
  off_t off_;
};

}  // namespace ots

#endif  // OTS_MEMORY_STREAM_H_
