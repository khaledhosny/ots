// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "test-context.h"

namespace {

class FileStream : public ots::OTSStream {
 public:
  explicit FileStream(std::string& filename)
      : file_(false), off_(0) {
    if (!filename.empty()) {
      stream_.open(filename.c_str(), std::ofstream::out | std::ofstream::binary);
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
  std::string in_filename;
  std::string out_filename;
  int font_index = -1;
  bool verbose = false;
  bool version = false;

  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg.at(0) == '-') {
      if (arg == "--version") {
        version = true;
      } else if (arg == "--verbose") {
        verbose = true;
      } else {
        std::cerr << "Unrecognized option: " << arg << std::endl;
        return 1;
      }
    } else if (in_filename.empty()) {
      in_filename = arg;
    } else if (out_filename.empty()) {
      out_filename = arg;
    } else if (font_index == -1) {
      font_index = std::strtol(arg.c_str(), NULL, 10);
    } else {
      std::cerr << "Unrecognized argument: " << arg << std::endl;
      return 1;
    }
  }

  if (version) {
    std::cout << PACKAGE_STRING << std::endl;
    return 0;
  }

  if (in_filename.empty()) {
    return Usage(argv[0]);
  }

  std::ifstream ifs(in_filename);
  if (!ifs.good()) {
    std::cerr << "Failed to open: " << in_filename << std::endl;
    return 1;
  }

  std::vector<uint8_t> in((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

  ots::TestContext context(-1);

  FileStream output(out_filename);
  const bool result = context.Process(&output, in.data(), in.size(), font_index);

  if (verbose) {
    if (result)
      std::cout << "File sanitized successfully!\n";
    else
      std::cout << "Failed to sanitize file!\n";
  }

  return !result;
}
