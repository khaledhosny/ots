// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

namespace {

int Usage(const char *argv0) {
  std::fprintf(stderr, "Usage: %s <ttf file>\n", argv0);
  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) return Usage(argv[0]);

  const int fd = ::open(argv[1], O_RDONLY);
  if (fd < 0) {
    ::perror("open");
    return 1;
  }

  struct stat st;
  ::fstat(fd, &st);

  uint8_t *data = new uint8_t[st.st_size];
  if (::read(fd, data, st.st_size) != st.st_size) {
    ::close(fd);
    std::fprintf(stderr, "Failed to read file!\n");
    return 1;
  }
  ::close(fd);

  // A transcoded font is usually smaller than an original font.
  // However, it can be slightly bigger than the original one due to
  // name table replacement and/or padding for glyf table.
  static const size_t kPadLen = 20 * 1024;
  uint8_t *result = new uint8_t[st.st_size + kPadLen];
  ots::MemoryStream output(result, st.st_size + kPadLen);

  bool r = ots::Process(&output, data, st.st_size);
  if (!r) {
    std::fprintf(stderr, "Failed to sanitise file!\n");
    return 1;
  }
  const size_t result_len = output.Tell();
  free(data);

  uint8_t *result2 = new uint8_t[result_len];
  ots::MemoryStream output2(result2, result_len);
  r = ots::Process(&output2, result, result_len);
  if (!r) {
    std::fprintf(stderr, "Failed to sanitise previous output!\n");
    return 1;
  }
  const size_t result2_len = output2.Tell();

  bool dump_results = false;
  if (result2_len != result_len) {
    std::fprintf(stderr, "Outputs differ in length\n");
    dump_results = true;
  } else if (std::memcmp(result2, result, result_len)) {
    std::fprintf(stderr, "Outputs differ in content\n");
    dump_results = true;
  }

  if (dump_results) {
    std::fprintf(stderr, "Dumping results to out1.tff and out2.tff\n");
    int fd1 = ::open("out1.ttf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int fd2 = ::open("out2.ttf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd1 < 0 || fd2 < 0) {
      ::perror("opening output file");
      return 1;
    }
    if ((::write(fd1, result, result_len) < 0) ||
        (::write(fd2, result2, result2_len) < 0)) {
      ::perror("writing output file");
      return 1;
    }
  }

  return 0;
}
