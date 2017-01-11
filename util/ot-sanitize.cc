// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "test-context.h"

namespace {

class FileStream : public ots::OTSStream {
 public:
  explicit FileStream(std::string& filename)
      : file_(false), off_(0) {
    if (!filename.empty()) {
      stream_.open(filename, std::ofstream::out | std::ofstream::binary);
      file_ = true;
    }
  }

  bool WriteRaw(const void *data, size_t length) {
    off_ += length;
    if (file_) {
      stream_.write(static_cast<const char*>(data), length);
      return stream_.good();
    }
    return true;
  }

  bool Seek(off_t position) {
    off_ = position;
    if (file_) {
      stream_.seekp(position);
      return stream_.good();
    }
    return true;
  }

  off_t Tell() const {
    return off_;
  }

 private:
  std::ofstream stream_;
  bool file_;
  off_t off_;
};

int Usage(const char *argv0) {
  std::fprintf(stderr, "Usage: %s font_file [dest_font_file] [index]\n", argv0);
  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 4) return Usage(argv[0]);

  if (std::strcmp("--version", argv[1]) == 0) {
    std::printf("%s\n", PACKAGE_STRING);
    return 0;
  }

  std::ifstream ifs(argv[1]);
  if (!ifs.good())
    return 1;

  std::vector<uint8_t> in((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

  ots::TestContext context(-1);

  uint32_t index = -1;
  if (argc >= 4)
    index = strtol(argv[3], NULL, 0);

  std::string out("");
  if (argc >= 3)
    out = argv[2];

  FileStream output(out);
  const bool result = context.Process(&output, in.data(), in.size(), index);

  if (!result)
    std::cerr << "Failed to sanitize file!\n";

  return !result;
}
