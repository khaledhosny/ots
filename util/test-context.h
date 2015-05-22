// Copyright (c) 2014-2015 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_TEST_CONTEXT_H_
#define OTS_TEST_CONTEXT_H_

#include "opentype-sanitiser.h"

namespace ots {

class TestContext: public ots::OTSContext {
 public:
  TestContext(unsigned level)
    : level_(level)
  { }

  virtual void Message(int level, const char *format, ...) {
    va_list va;

    if (level > level_)
      return;

    if (level == 0)
      std::fprintf(stderr, "ERROR: ");
    else
      std::fprintf(stderr, "WARNING: ");
    va_start(va, format);
    std::vfprintf(stderr, format, va);
    std::fprintf(stderr, "\n");
    va_end(va);
  }

  virtual ots::TableAction GetTableAction(uint32_t tag) {
    switch (tag) {
      case OTS_TAG('S','i','l','f'):
      case OTS_TAG('S','i','l','l'):
      case OTS_TAG('G','l','o','c'):
      case OTS_TAG('G','l','a','t'):
      case OTS_TAG('F','e','a','t'):
      case OTS_TAG('C','B','D','T'):
      case OTS_TAG('C','B','L','C'):
        return ots::TABLE_ACTION_PASSTHRU;
      default:
        return ots::TABLE_ACTION_DEFAULT;
    }
  }

private:
  unsigned level_;
};

}  // namespace ots

#endif  // OTS_TEST_CONTEXT_H_
