// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#ifdef OTS_FUZZER_MAIN
#include <fstream>
#include <iostream>
#include <iterator>
#endif

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ots::OTSContext context;
  ots::MemoryStream stream((void*)data, size);
  context.Process(&stream, data, size);
  return 0;
}

#ifdef OTS_FUZZER_MAIN
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
