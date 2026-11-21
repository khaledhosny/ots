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

void WriteU32(uint8_t* p, uint32_t value) {
  p[0] = value >> 24;
  p[1] = (value >> 16) & 0xff;
  p[2] = (value >> 8) & 0xff;
  p[3] = value & 0xff;
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

const uint32_t kHvarTag = OTS_TAG('H', 'V', 'A', 'R');

// The test font has a 1136-byte HVAR table whose advance width mapping is a
// format 0 DeltaSetIndexMap at offset 820 with 312 1-byte entries, ending at
// the end of the table.
class HvarTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("OTS_TEST_FONT");
    ASSERT_NE(path, nullptr) << "OTS_TEST_FONT is not set";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good()) << "Can't read " << path;
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    uint32_t length = 0;
    hvar = FindTable(input, kHvarTag, &length);
    ASSERT_TRUE(hvar);
    ASSERT_EQ(length, 1136u);
    ASSERT_EQ(U32(hvar + 8), 820u);  // advanceWidthMappingOffset
    map = hvar + 820;
    ASSERT_EQ(map[0], 0);          // format
    ASSERT_EQ(map[1] & 0x30, 0);   // entryFormat: 1-byte entries
    ASSERT_EQ(U16(map + 2), 312);  // mapCount
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

  // Rewrite the map header as format 1, whose mapCount is 32-bit, leaving
  // 310 bytes of map data before the end of the table.
  void MakeFormat1(uint32_t map_count) {
    map[0] = 1;
    WriteU32(map + 2, map_count);
  }

  // Dropping HVAR drops all variation tables, which in turn makes this font's
  // GDEF (it has an ItemVariationStore) and so the whole font fail. Either
  // way the HVAR table must not reach the output.
  bool OutputHasHvar() {
    return Sanitize() && FindTable(output, kHvarTag);
  }

  std::vector<uint8_t> input;
  std::vector<uint8_t> output;
  uint8_t* hvar = nullptr;
  uint8_t* map = nullptr;
};

TEST_F(HvarTest, KeepsValidTable) {
  EXPECT_TRUE(OutputHasHvar());
}

TEST_F(HvarTest, KeepsValidFormat1Map) {
  MakeFormat1(310);
  EXPECT_TRUE(OutputHasHvar());
}

TEST_F(HvarTest, DropsTruncatedFormat1Map) {
  MakeFormat1(0xffff);
  EXPECT_FALSE(OutputHasHvar());
}

TEST_F(HvarTest, DropsUnknownMapFormat) {
  map[0] = 2;
  EXPECT_FALSE(OutputHasHvar());
}

}  // namespace
