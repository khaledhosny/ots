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
bool FileToString(const std::string &path, std::string &string) {
  std::ifstream f(path.c_str());
  if (f.good()) {
    string = std::string(std::istreambuf_iterator<char>(f),
                         std::istreambuf_iterator<char>());
    return true;
  }
  return false;
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    std::string s;
    if (FileToString(argv[i], s)) {
      std::cout << argv[i] << std::endl;
      LLVMFuzzerTestOneInput((const uint8_t*)s.data(), s.size());
    } else {
      std::cerr << "Failed to read file: " << argv[i] << std::endl;
      return 1;
    }
  }
  return 0;
}
#endif
