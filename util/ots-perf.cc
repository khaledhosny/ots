// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <fstream>
#include <iostream>
#include <vector>

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

  // load the font to memory.
  std::ifstream ifs(argv[1], std::ifstream::binary);
  if (!ifs.good()) {
    std::fprintf(stderr, "Failed to read file!\n");
    return 1;
  }

  std::vector<uint8_t> in((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

  // A transcoded font is usually smaller than an original font.
  // However, it can be slightly bigger than the original one due to
  // name table replacement and/or padding for glyf table.
  std::vector<uint8_t> result(in.size() * 8);

  int num_repeat = 250;
  if (in.size() < 1024 * 1024) {
    num_repeat = 2500;
  }
  if (in.size() < 1024 * 100) {
    num_repeat = 5000;
  }

  struct timeval start, end, elapsed;
  ::gettimeofday(&start, 0);
  for (int i = 0; i < num_repeat; ++i) {
    ots::MemoryStream output(result.data(), result.size());
    ots::OTSContext context;
    bool r = context.Process(&output, in.data(), in.size());
    if (!r) {
      std::fprintf(stderr, "Failed to sanitize file!\n");
      return 1;
    }
  }

  ::gettimeofday(&end, 0);
  timersub(&end, &start, &elapsed);

  long long unsigned us
      = ((elapsed.tv_sec * 1000 * 1000) + elapsed.tv_usec) / num_repeat;
  std::fprintf(stderr, "%llu [us] %s (%llu bytes, %llu [byte/us])\n",
               us, argv[1], static_cast<long long>(in.size()),
               (us ? in.size() / us : 0));

  return 0;
}
