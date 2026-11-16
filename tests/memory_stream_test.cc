// Copyright (c) 2026 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression tests for ots::ExpandingMemoryStream growth.
// An initial size above the limit used to make the first growth copy the
// old, larger buffer into a new buffer clamped to the limit, overflowing it.

#include <string>

#include <gtest/gtest.h>

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

namespace {

TEST(ExpandingMemoryStreamTest, InitialAboveLimitDoesNotOverflow) {
  const size_t limit = 1024;
  ots::ExpandingMemoryStream stream(4 * limit, limit);
  EXPECT_EQ(stream.size(), limit);

  // Fill the stream up to the limit, then keep writing: the stream must
  // refuse instead of honouring the larger initial size and, once that is
  // exhausted, reallocating a smaller buffer and copying past its end.
  const std::string data(4 * limit, 'x');
  EXPECT_TRUE(stream.WriteRaw(data.data(), limit));
  EXPECT_EQ(static_cast<size_t>(stream.Tell()), limit);
  EXPECT_FALSE(stream.WriteRaw(data.data(), 3 * limit));
  EXPECT_FALSE(stream.WriteRaw(data.data(), 1));
  EXPECT_EQ(static_cast<size_t>(stream.Tell()), limit);
}

TEST(ExpandingMemoryStreamTest, GrowsUpToLimit) {
  const size_t limit = 1024;
  ots::ExpandingMemoryStream stream(16, limit);
  EXPECT_EQ(stream.size(), limit);

  const std::string data(100, 'x');
  size_t written = 0;
  while (written + data.size() <= limit) {
    EXPECT_TRUE(stream.WriteRaw(data.data(), data.size()));
    written += data.size();
  }
  EXPECT_EQ(static_cast<size_t>(stream.Tell()), written);
  EXPECT_FALSE(stream.WriteRaw(data.data(), data.size()));
  EXPECT_TRUE(stream.WriteRaw(data.data(), limit - written));
  EXPECT_EQ(static_cast<size_t>(stream.Tell()), limit);
  EXPECT_FALSE(stream.WriteRaw(data.data(), 1));
}

}  // namespace
