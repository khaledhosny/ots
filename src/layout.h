// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_LAYOUT_H_
#define OTS_LAYOUT_H_

#include "ots.h"

// Utility functions for OpenType layout common table formats.
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace ots {

bool ParseClassDefTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes);

bool ParseCoverageTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs);

}  // namespace ots

#endif  // OTS_LAYOUT_H_

