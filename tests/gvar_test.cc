// Copyright (c) 2026 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

uint8_t* FindTable(std::vector<uint8_t>& font, uint32_t tag,
                   uint32_t* length = nullptr) {
  if (font.size() < 12)
    return nullptr;
  const uint16_t num_tables = U16(font.data() + 4);
  if (12 + num_tables * 16u > font.size())
    return nullptr;
  for (unsigned i = 0; i < num_tables; ++i) {
    const uint8_t* record = font.data() + 12 + i * 16;
    if (U32(record) == tag) {
      const uint32_t offset = U32(record + 8);
      const uint32_t table_length = U32(record + 12);
      if (offset > font.size() || table_length > font.size() - offset)
        return nullptr;
      if (length)
        *length = table_length;
      return font.data() + offset;
    }
  }
  return nullptr;
}

class SilentContext : public ots::OTSContext {
 public:
  void Message(int, const char*, ...) override {}
};

const uint32_t kGvarTag = OTS_TAG('g', 'v', 'a', 'r');

// The test font has a gvar table with 19 glyphs and short (16-bit) offsets.
// Its GlyphVariationData array starts 180 bytes into the table and is 8698
// bytes long, so the last offset is 8698.
const unsigned kGlyphCount = 19;
const unsigned kDataArrayOffset = 180;
const unsigned kDataLength = 8698;

class GvarTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("OTS_TEST_FONT");
    ASSERT_NE(path, nullptr) << "OTS_TEST_FONT is not set";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good()) << "Can't read " << path;
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    gvar = FindTable(input, kGvarTag);
    ASSERT_TRUE(gvar);
    ASSERT_EQ(U16(gvar + 12), kGlyphCount);           // glyphCount
    ASSERT_EQ(U16(gvar + 14), 0);                     // flags, short offsets
    ASSERT_EQ(U32(gvar + 16), kDataArrayOffset);      // data array offset
    ASSERT_EQ(U16(Offset(kGlyphCount)) * 2u, kDataLength);
  }

  // The i-th entry of glyphVariationDataOffsets, which holds offset / 2.
  uint8_t* Offset(unsigned i) { return gvar + 20 + i * 2; }

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
  uint8_t* gvar = nullptr;
};

TEST_F(GvarTest, KeepsValidTable) {
  ASSERT_TRUE(Sanitize());
  uint32_t length = 0;
  const uint8_t* out = FindTable(output, kGvarTag, &length);
  ASSERT_TRUE(out);
  EXPECT_EQ(U16(out + 20 + kGlyphCount * 2) * 2u, length - kDataArrayOffset);
}

TEST_F(GvarTest, DropsOffsetPastVariationData) {
  // The last offset bounds the final glyph's data, so 0xffff * 2 puts the end
  // of glyph 18 more than 100KB past the GlyphVariationData array.
  WriteU16(Offset(kGlyphCount), 0xffff);
  ASSERT_TRUE(Sanitize());
  EXPECT_FALSE(FindTable(output, kGvarTag));
}

TEST_F(GvarTest, DropsOutOfOrderOffsets) {
  // Glyph 18's data starts at 8598, so ending it at 50 makes its length
  // negative.
  WriteU16(Offset(kGlyphCount), 25);
  ASSERT_TRUE(Sanitize());
  EXPECT_FALSE(FindTable(output, kGvarTag));
}

}  // namespace
