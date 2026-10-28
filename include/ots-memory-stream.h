// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
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

  size_t size() override { return length_; }

  bool WriteRaw(const void *data, size_t length) override {
    if ((off_ + length > length_) ||
        (length > std::numeric_limits<size_t>::max() - off_)) {
      return false;
    }
    std::memcpy(static_cast<char*>(ptr_) + off_, data, length);
    off_ += static_cast<off_t>(length);
    return true;
  }

  bool Seek(off_t position) override {
    if (position < 0) return false;
    if (static_cast<size_t>(position) > length_) return false;
    off_ = position;
    return true;
  }

  off_t Tell() const override {
    return off_;
  }

 private:
  void* const ptr_;
  size_t length_;
  off_t off_;
};

class ExpandingMemoryStream : public OTSStream {
 public:
  ExpandingMemoryStream(size_t initial, size_t limit)
      : length_(initial), limit_(limit), off_(0), high_(0) {
    // Zero-initialised: a caller that adopts this buffer may read regions
    // that serialisation seeked over and never wrote (see Seek() below).
    ptr_ = new uint8_t[length_]();
  }

  ~ExpandingMemoryStream() {
    delete[] static_cast<uint8_t*>(ptr_);
  }

  void* get() const {
    return ptr_;
  }

  size_t size() override { return limit_; }

  bool WriteRaw(const void *data, size_t length) override {
    if ((off_ + length > length_) ||
        (length > std::numeric_limits<size_t>::max() - off_)) {
      if (length_ == limit_)
        return false;
      size_t new_length = (length_ + 1) * 2;
      if (new_length < length_)
        return false;
      if (new_length > limit_)
        new_length = limit_;
      uint8_t* new_buf = new uint8_t[new_length]();
      std::memcpy(new_buf, ptr_, length_);
      length_ = new_length;
      delete[] static_cast<uint8_t*>(ptr_);
      ptr_ = new_buf;
      return WriteRaw(data, length);
    }
    std::memcpy(static_cast<char*>(ptr_) + off_, data, length);
    off_ += static_cast<off_t>(length);
    if (off_ > high_) high_ = off_;
    return true;
  }

  bool Seek(off_t position) override {
    if (position < 0) return false;
    if (static_cast<size_t>(position) > length_) return false;
    // Serialisation seeks backwards to patch already-written fields (e.g.
    // checksums) and then forwards again, so only the span above the
    // high-water mark has genuinely never been written. Zero just that span:
    // the buffer is handed to the caller in full, and a never-written gap
    // would otherwise expose whatever the allocator previously held.
    if (position > high_) {
      std::memset(static_cast<char*>(ptr_) + high_, 0,
                  static_cast<size_t>(position - high_));
      high_ = position;
    }
    off_ = position;
    return true;
  }

  off_t Tell() const override {
    return off_;
  }

 private:
  void* ptr_;
  size_t length_;
  const size_t limit_;
  off_t off_;
  off_t high_;  // highest offset ever written; everything above is unwritten
};

}  // namespace ots

#endif  // OTS_MEMORY_STREAM_H_
