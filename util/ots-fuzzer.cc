// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#ifndef OTS_FUZZER_NO_MAIN
#include <fstream>
#include <iostream>
#include <iterator>
#endif

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

static uint8_t buffer[256 * 1024] = { 0 };

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ots::OTSContext context;
  ots::MemoryStream stream(static_cast<void*>(buffer), sizeof(buffer));
  context.Process(&stream, data, size);
  return 0;
}

#ifndef OTS_FUZZER_NO_MAIN
int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    std::cout << argv[i] << std::endl;

    std::ifstream f(argv[i]);
    if (!f.good())
      return 1;

    std::string s((std::istreambuf_iterator<char>(f)),
                  (std::istreambuf_iterator<char>()));
    LLVMFuzzerTestOneInput((const uint8_t*)s.data(), s.size());
  }
  return 0;
}
#endif
