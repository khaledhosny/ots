// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A very simple driver program while sanitises the file given as argv[1] and
// writes the sanitised version to stdout.

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
#include "opentype-sanitiser.h"

#if defined(_WIN32)
#define ADDITIONAL_OPEN_FLAGS O_BINARY
#else
#define ADDITIONAL_OPEN_FLAGS 0
#endif

namespace {

int Usage(const char *argv0) {
  std::fprintf(stderr, "Usage: %s ttf_file [dest_ttf_file]\n", argv0);
  return 1;
}

bool Message(void *aUserData, const char *format, ...) {
  va_list va;

  va_start(va, format);
  std::vfprintf(stderr, format, va);
  std::fprintf(stderr, "\n");
  va_end(va);

  return false;
}

uint32_t Tag(const char *tag_str) {
  uint32_t ret;
  std::memcpy(&ret, tag_str, 4);
  return ret;
}

ots::TableAction TableActionCallback(uint32_t tag, void *user_data) {
  if (tag == Tag("Silf") ||
      tag == Tag("Sill") ||
      tag == Tag("Gloc") ||
      tag == Tag("Glat") ||
      tag == Tag("Feat")) {
    return ots::TABLE_ACTION_PASSTHRU;
  }
  return ots::TABLE_ACTION_DEFAULT;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) return Usage(argv[0]);

  ots::EnableWOFF2();

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
    return 1;
  }
  ::close(fd);

  ots::SetMessageCallback(&Message, NULL);
  ots::SetTableActionCallback(&TableActionCallback, NULL);

  FILE* out = NULL;
  if (argc == 3)
    out = fopen(argv[2], "wb");

  ots::FILEStream output(out);
  const bool result = ots::Process(&output, data, st.st_size);

  if (!result) {
    std::fprintf(stderr, "Failed to sanitise file!\n");
  }
  return !result;
}
