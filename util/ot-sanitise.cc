// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A very simple driver program while sanitises the file given as argv[1] and
// writes the sanitised version to stdout.

#include "config.h"

#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif  // defined(_WIN32)

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "file-stream.h"
#include "test-context.h"

#if defined(_WIN32)
#define ADDITIONAL_OPEN_FLAGS O_BINARY
#else
#define ADDITIONAL_OPEN_FLAGS 0
#endif

namespace {


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

  const int fd = ::open(argv[1], O_RDONLY | ADDITIONAL_OPEN_FLAGS);
  if (fd < 0) {
    ::perror("open");
    return 1;
  }

  struct stat st;
  ::fstat(fd, &st);

  uint8_t *data = new uint8_t[st.st_size];
  if (::read(fd, data, st.st_size) != st.st_size) {
    ::perror("read");
    delete[] data;
    return 1;
  }
  ::close(fd);

  ots::TestContext context(-1);

  FILE* out = NULL;
  if (argc >= 3)
    out = fopen(argv[2], "wb");

  uint32_t index = -1;
  if (argc >= 4)
    index = strtol(argv[3], NULL, 0);

  ots::FILEStream output(out);
  const bool result = context.Process(&output, data, st.st_size, index);

  if (!result) {
    std::fprintf(stderr, "Failed to sanitise file!\n");
  }

  delete[] data;
  return !result;
}
