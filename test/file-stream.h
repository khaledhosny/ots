// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_FILE_STREAM_H_
#define OTS_FILE_STREAM_H_

#include "opentype-sanitiser.h"

namespace ots {

class FILEStream : public OTSStream {
 public:
  explicit FILEStream(FILE *stream)
      : file_(stream) {
  }

  ~FILEStream() {
  }

  bool WriteRaw(const void *data, size_t length) {
    return fwrite(data, length, 1, file_) == 1;
  }

  bool Seek(off_t position) {
    return fseek(file_, position, SEEK_SET) == 0;
  }

  off_t Tell() const {
    return ftell(file_);
  }

 private:
  FILE * const file_;
};

}  // namespace ots

#endif  // OTS_FILE_STREAM_H_
