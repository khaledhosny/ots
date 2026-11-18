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

const uint32_t kVdmxTag = OTS_TAG('V', 'D', 'M', 'X');

// The test font has a VDMX table with one ratio pointing at one group of 248
// records at offset 12.
class VdmxTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("OTS_TEST_FONT");
    ASSERT_NE(path, nullptr) << "OTS_TEST_FONT is not set";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good()) << "Can't read " << path;
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    vdmx = FindTable(input, kVdmxTag);
    ASSERT_TRUE(vdmx);
    ASSERT_EQ(U16(vdmx + 2), 1);   // numRecs
    ASSERT_EQ(U16(vdmx + 4), 1);   // numRatios
    ASSERT_EQ(U16(vdmx + 10), 12);  // offset of the only group
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
  uint8_t* vdmx = nullptr;
};

TEST_F(VdmxTest, KeepsValidTable) {
  ASSERT_TRUE(Sanitize());
  uint32_t length = 0;
  const uint8_t* out = FindTable(output, kVdmxTag, &length);
  ASSERT_TRUE(out);
  EXPECT_EQ(U16(out + 10), 12);
  EXPECT_LT(U16(out + 10), length);
}

TEST_F(VdmxTest, DropsOffsetPastSerializedGroups) {
  // Shrink the group so it ends at offset 616, and point the ratio at bytes
  // after it that are not serialized.
  WriteU16(vdmx + 12, 100);
  WriteU16(vdmx + 10, 1000);
  ASSERT_TRUE(Sanitize());
  EXPECT_FALSE(FindTable(output, kVdmxTag));
}

TEST_F(VdmxTest, DropsOffsetIntoGroup) {
  // Point the ratio at the middle of the group.
  WriteU16(vdmx + 10, 16);
  ASSERT_TRUE(Sanitize());
  EXPECT_FALSE(FindTable(output, kVdmxTag));
}

}  // namespace
