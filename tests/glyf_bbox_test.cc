// Copyright (c) 2026 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression tests for https://bugzilla.mozilla.org/show_bug.cgi?id=2091381

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

namespace {

uint16_t U16(const uint8_t* p) {
  return (p[0] << 8) | p[1];
}

uint32_t U32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) |
         p[3];
}

void WriteU16(uint8_t* p, uint16_t value) {
  p[0] = value >> 8;
  p[1] = value & 0xff;
}

uint8_t* FindTable(std::vector<uint8_t>& font, uint32_t tag) {
  if (font.size() < 12)
    return nullptr;
  const uint16_t num_tables = U16(font.data() + 4);
  if (12 + num_tables * 16u > font.size())
    return nullptr;
  for (unsigned i = 0; i < num_tables; ++i) {
    const uint8_t* record = font.data() + 12 + i * 16;
    if (U32(record) == tag) {
      const uint32_t offset = U32(record + 8);
      const uint32_t length = U32(record + 12);
      if (offset > font.size() || length > font.size() - offset)
        return nullptr;
      return font.data() + offset;
    }
  }
  return nullptr;
}

class SilentContext : public ots::OTSContext {
 public:
  void Message(int, const char*, ...) override {}
};

// Test glyph 1 in a full metric (2) or the trailing LSB array (1).
class GlyfBboxTest : public ::testing::TestWithParam<uint16_t> {
 protected:
  void SetUp() override {
    const char* path = std::getenv("OTS_TEST_FONT");
    ASSERT_NE(path, nullptr) << "OTS_TEST_FONT is not set";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good()) << "Can't read " << path;
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    // Glyph 1's outline spans [100, 900], but its stored xMin is 300.
    uint8_t* hhea = FindTable(input, OTS_TAG('h', 'h', 'e', 'a'));
    ASSERT_TRUE(hhea);
    ASSERT_EQ(U16(hhea + 34), 2);
    // With one full metric, glyph 1 shares glyph 0's advance width.
    WriteU16(hhea + 34, GetParam());
    lsb_offset = GetParam() == 1 ? 4 : 6;
  }

  bool Sanitize() {
    SilentContext context;
    ots::ExpandingMemoryStream stream(input.size(), input.size() * 8);
    if (!context.Process(&stream, input.data(), input.size()))
      return false;
    const uint8_t* data = static_cast<const uint8_t*>(stream.get());
    output.assign(data, data + stream.Tell());
    return true;
  }

  std::vector<uint8_t> input;
  std::vector<uint8_t> output;
  size_t lsb_offset;
};

TEST_P(GlyfBboxTest, PreservesPositionWhenRepairingBbox) {
  uint8_t* input_hmtx = FindTable(input, OTS_TAG('h', 'm', 't', 'x'));
  ASSERT_TRUE(input_hmtx);
  // After subtracting 200, -32768 fits but -32769 must fail.
  const int16_t bearings[] = {300, -32568, -32569, 32767};
  for (int16_t bearing : bearings) {
    SCOPED_TRACE(bearing);
    WriteU16(input_hmtx + lsb_offset, static_cast<uint16_t>(bearing));
    const int expected_lsb = bearing - 200;
    const bool fits = expected_lsb >= -32768;
    ASSERT_EQ(Sanitize(), fits);
    if (!fits)
      continue;

    const uint8_t* head = FindTable(output, OTS_TAG('h', 'e', 'a', 'd'));
    const uint8_t* hhea = FindTable(output, OTS_TAG('h', 'h', 'e', 'a'));
    const uint8_t* hmtx = FindTable(output, OTS_TAG('h', 'm', 't', 'x'));
    const uint8_t* loca = FindTable(output, OTS_TAG('l', 'o', 'c', 'a'));
    const uint8_t* glyf = FindTable(output, OTS_TAG('g', 'l', 'y', 'f'));
    ASSERT_TRUE(head && hhea && hmtx && loca && glyf);

    const uint32_t glyph_offset =
        U16(head + 50) == 1 ? U32(loca + 4) : U16(loca + 2) * 2u;
    EXPECT_EQ(U16(glyf + glyph_offset + 2), 100);
    EXPECT_EQ(U16(hhea + 34), GetParam());
    EXPECT_EQ(U16(hmtx), 500);
    EXPECT_EQ(U16(hmtx + 2), 0);
    EXPECT_EQ(static_cast<int16_t>(U16(hmtx + lsb_offset)), expected_lsb);
  }
}

INSTANTIATE_TEST_SUITE_P(MetricsLayouts, GlyfBboxTest, ::testing::Values(1, 2));

}  // namespace
