// Copyright (c) 2026 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression tests for https://bugzilla.mozilla.org/show_bug.cgi?id=2091381

#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

namespace {

// Both fixtures use the same simple outline.
const std::array<int16_t, 4> kOutlineBounds = {100, 0, 900, 800};

uint16_t U16(const uint8_t* p) {
  return (p[0] << 8) | p[1];
}

uint32_t U32(const uint8_t* p) {
  return (static_cast<uint32_t>(U16(p)) << 16) | U16(p + 2);
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

uint8_t* Glyph(std::vector<uint8_t>& font, uint16_t gid) {
  const uint8_t* head = FindTable(font, OTS_TAG('h', 'e', 'a', 'd'));
  const uint8_t* loca = FindTable(font, OTS_TAG('l', 'o', 'c', 'a'));
  uint8_t* glyf = FindTable(font, OTS_TAG('g', 'l', 'y', 'f'));
  if (!head || !loca || !glyf)
    return nullptr;
  const uint32_t offset =
      U16(head + 50) == 1 ? U32(loca + 4 * gid) : U16(loca + 2 * gid) * 2u;
  return glyf + offset;
}

uint8_t* SideBearing(std::vector<uint8_t>& font, uint16_t gid) {
  const uint8_t* hhea = FindTable(font, OTS_TAG('h', 'h', 'e', 'a'));
  uint8_t* hmtx = FindTable(font, OTS_TAG('h', 'm', 't', 'x'));
  if (!hhea || !hmtx)
    return nullptr;
  const uint16_t num_metrics = U16(hhea + 34);
  return hmtx + (gid < num_metrics ? 4 * gid + 2
                                 : 4 * num_metrics + 2 * (gid - num_metrics));
}

class SilentContext : public ots::OTSContext {
 public:
  void Message(int, const char*, ...) override {}
};

class GlyfBboxTestBase : public ::testing::Test {
 protected:
  void LoadFont(const char* variable) {
    const char* path = std::getenv(variable);
    ASSERT_NE(path, nullptr) << variable << " is not set";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good()) << "Can't read " << path;
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());
    ASSERT_NE(Glyph(input, 0), nullptr);
    ASSERT_NE(SideBearing(input, 0), nullptr);
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

  void ExpectGlyph(uint16_t gid, std::array<int16_t, 4> bbox, int16_t lsb) {
    SCOPED_TRACE(gid);
    const uint8_t* glyph = Glyph(output, gid);
    const uint8_t* bearing = SideBearing(output, gid);
    ASSERT_NE(glyph, nullptr);
    ASSERT_NE(bearing, nullptr);
    for (unsigned i = 0; i < bbox.size(); ++i) {
      EXPECT_EQ(static_cast<int16_t>(U16(glyph + 2 + 2 * i)), bbox[i])
          << "bbox[" << i << "]";
    }
    EXPECT_EQ(static_cast<int16_t>(U16(bearing)), lsb);
  }

  std::vector<uint8_t> input;
  std::vector<uint8_t> output;
};

// Test glyph 1 in a full metric (2) or the trailing LSB array (1).
class GlyfBboxTest : public GlyfBboxTestBase,
                     public ::testing::WithParamInterface<uint16_t> {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(LoadFont("OTS_TEST_FONT"));
    // Glyph 1's outline spans [100, 900], but its stored xMin is 300.
    // With one full metric, it shares glyph 0's advance width.
    WriteU16(FindTable(input, OTS_TAG('h', 'h', 'e', 'a')) + 34, GetParam());
  }
};

TEST_P(GlyfBboxTest, PreservesPositionWhenRepairingBbox) {
  // After subtracting 200, -32768 fits but -32769 must fail.
  const int16_t bearings[] = {300, -32568, -32569, 32767};
  for (int16_t bearing : bearings) {
    SCOPED_TRACE(bearing);
    WriteU16(SideBearing(input, 1), bearing);
    const int expected_lsb = bearing - 200;
    const bool fits = expected_lsb >= -32768;
    ASSERT_EQ(Sanitize(), fits);
    if (!fits)
      continue;

    ExpectGlyph(1, kOutlineBounds, expected_lsb);
    const uint8_t* hhea = FindTable(output, OTS_TAG('h', 'h', 'e', 'a'));
    const uint8_t* hmtx = FindTable(output, OTS_TAG('h', 'm', 't', 'x'));
    ASSERT_TRUE(hhea && hmtx);
    EXPECT_EQ(U16(hhea + 34), GetParam());
    EXPECT_EQ(U16(hmtx), 500);
    EXPECT_EQ(U16(hmtx + 2), 0);
  }
}

INSTANTIATE_TEST_SUITE_P(MetricsLayouts, GlyfBboxTest, ::testing::Values(1, 2));

// Each composite has one component, and every glyph starts with xMin = lsb.
class GlyfCompositeBboxTest : public GlyfBboxTestBase {
 protected:
  void SetUp() override {
    LoadFont("OTS_COMPOSITE_TEST_FONT");
  }
};

TEST_F(GlyfCompositeBboxTest, FixesCompositeBboxes) {
  const struct {
    uint16_t gid;
    std::array<int16_t, 4> bbox;
  } kCases[] = {
      {1, kOutlineBounds},  // Simple glyph, correct bbox.
      {2, kOutlineBounds},  // Composite, bbox too small.
      {3, kOutlineBounds},  // Composite, correct bbox.
      {4, kOutlineBounds},  // Simple glyph, stored xMin = 300.
      // Scale 0.5, offset (200, 100), no offset flag: cover both placements.
      // Unscaled placement: (250, 100, 650, 500); scaled: (150, 50, 550, 450).
      {5, {150, 50, 650, 500}},
      {6, {250, 100, 650, 500}},  // Same, with UNSCALED_COMPONENT_OFFSET.
      // Horizontal flip, offset (1000, 0), SCALED_COMPONENT_OFFSET:
      // FreeType's placement: (100, 0, 900, 800);
      // Apple's (offset transformed by the matrix): (-1900, 0, -1100, 800).
      {7, {-1900, 0, 900, 800}},
      {8, {-800, 100, 0, 900}},  // Rotate 90 degrees, unscaled offset.
      // Glyph 10 scaled by 0.5, unscaled offset (100, 100). Use its outline
      // bounds even though it hasn't been repaired yet.
      {9, {175, 100, 775, 300}},
      {10, {150, 0, 1350, 400}},  // Scale (1.5, 0.5), unscaled offset.
      // Horizontal flip, offset (1000, 0), no offset flag: only unscaled.
      {11, kOutlineBounds},
  };
  ASSERT_TRUE(Sanitize());
  for (const auto& c : kCases) {
    ExpectGlyph(c.gid, c.bbox, c.bbox[0]);
  }
}

TEST_F(GlyfCompositeBboxTest, ShiftsLsbWhenLoweringCompositeXMin) {
  // Set glyph 2's xMin and lsb to 300.
  WriteU16(Glyph(input, 2) + 2, 300);
  WriteU16(SideBearing(input, 2), 300);
  ASSERT_TRUE(Sanitize());
  ExpectGlyph(2, kOutlineBounds, 100);
}

TEST_F(GlyfCompositeBboxTest, UsesSignedByteOffsets) {
  // Translate glyph 2's component by (-10, -20).
  uint8_t* glyph = Glyph(input, 2);
  ASSERT_EQ(U16(glyph + 10) & 0x3, 0x2);  // ARGS_ARE_XY_VALUES, bytes.
  glyph[14] = static_cast<uint8_t>(-10);
  glyph[15] = static_cast<uint8_t>(-20);
  ASSERT_TRUE(Sanitize());
  ExpectGlyph(2, {90, -20, 890, 780}, 90);
}

TEST_F(GlyfCompositeBboxTest, KeepsBboxOfPointMatchedComponent) {
  // Bbox repair skips point-matched components.
  uint8_t* glyph = Glyph(input, 2);
  WriteU16(glyph + 10, U16(glyph + 10) & ~0x2);
  ASSERT_TRUE(Sanitize());
  ExpectGlyph(2, {100, 0, 500, 400}, 100);
}

}  // namespace
