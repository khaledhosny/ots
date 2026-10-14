// Copyright (c) 2026 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gtest/gtest.h>

#include "cmap.h"
#include "gvar.h"
#include "glyf.h"
#include "head.h"
#include "hhea.h"
#include "hmtx.h"
#include "loca.h"
#include "name.h"
#include "maxp.h"
#include "layout.h"
#include "ots-memory-stream.h"
#include "varc.h"

namespace {

using Bytes = std::vector<uint8_t>;

void U16(Bytes& b, uint16_t v) {
  b.push_back(v >> 8);
  b.push_back(v);
}

void U24(Bytes& b, uint32_t v) {
  b.push_back(v >> 16);
  U16(b, v);
}

void U32(Bytes& b, uint32_t v) {
  U16(b, v >> 16);
  U16(b, v);
}

void Set32(Bytes& b, size_t at, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) b[at + i] = v >> (24 - i * 8);
}

class QuietContext : public ots::OTSContext {
 public:
  void Message(int, const char*, ...) override {}
};

class OFFTest : public ::testing::Test {
 protected:
  OFFTest() : font(&file) {
    file.context = &context;
    auto* maxp = new ots::OpenTypeMAXP(&font, OTS_TAG_MAXP);
    maxp->num_glyphs = 2;
    maxp->version_1 = false;
    Add(maxp);
  }

  void Add(ots::Table* table) {
    ots::TableEntry entry{};
    entry.tag = table->Tag();
    font.AddTable(entry, table);
  }

  void AddInternalAxis() {
    font.has_varc = true;
    auto* gvar = new ots::OpenTypeGVAR(&font, OTS_TAG_GVAR);
    // One axis, two glyphs, no shared tuples or glyph variation data.
    internal_gvar = {0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                     0, 2, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0, 0, 0};
    ASSERT_TRUE(gvar->Parse(internal_gvar.data(), internal_gvar.size()));
    ASSERT_TRUE(gvar->ShouldSerialize());
    Add(gvar);
  }

  Bytes Varc(const Bytes& store) {
    Bytes b;
    U32(b, 0x00010000);
    U32(b, 24);  // Coverage.
    U32(b, 37);  // MultiItemVariationStore.
    U32(b, 0);   // Conditions.
    U32(b, 0);   // Axis indices.
    U32(b, 30);  // Empty glyph record.
    U16(b, 1); U16(b, 1); U16(b, 0);
    U32(b, 1); b.insert(b.end(), {1, 1, 1});
    b.insert(b.end(), store.begin(), store.end());
    return b;
  }

  bool ParseVarc(const Bytes& b) {
    ots::OpenTypeVARC varc(&font, OTS_TAG_VARC);
    return varc.Parse(b.data(), b.size());
  }

  QuietContext context;
  ots::FontFile file;
  ots::Font font;
  Bytes internal_gvar;
};

TEST_F(OFFTest, ExtendedCoverageAndClasses) {
  Bytes coverage;
  U16(coverage, 3); U24(coverage, 2); U24(coverage, 65536); U24(coverage, 70000);
  uint32_t count = 0, last = 0;
  EXPECT_TRUE(ots::ParseCoverageTable(&font, coverage.data(), coverage.size(),
                                     70001, 2, &count, &last));
  EXPECT_EQ(2u, count);
  EXPECT_EQ(70000u, last);
  EXPECT_FALSE(ots::ParseCoverageTable(&font, coverage.data(), coverage.size(), 70000));
  coverage.pop_back();
  EXPECT_FALSE(ots::ParseCoverageTable(&font, coverage.data(), coverage.size(), 70001));

  Bytes ranges;
  U16(ranges, 4); U24(ranges, 1);
  U24(ranges, 65536); U24(ranges, 70000); U24(ranges, 0);
  EXPECT_TRUE(ots::ParseCoverageTable(&font, ranges.data(), ranges.size(),
                                     70001, 4465, &count, &last));
  EXPECT_EQ(4465u, count);
  EXPECT_EQ(70000u, last);

  Bytes classes;
  U16(classes, 3); U24(classes, 65536); U24(classes, 2);
  U24(classes, 1); U24(classes, 4);
  EXPECT_TRUE(ots::ParseClassDefTable(&font, classes.data(), classes.size(), 70001, 4));
  EXPECT_FALSE(ots::ParseClassDefTable(&font, classes.data(), classes.size(), 65537, 4));
  classes.clear();
  U16(classes, 4); U24(classes, 1);
  U24(classes, 65536); U24(classes, 70000); U16(classes, 4);
  EXPECT_TRUE(ots::ParseClassDefTable(&font, classes.data(), classes.size(), 70001, 4));
  EXPECT_FALSE(ots::ParseClassDefTable(&font, classes.data(), classes.size(), 70000, 4));
}

TEST_F(OFFTest, ExtendedProfileAndMetrics) {
  Bytes profile;
  U32(profile, 0x00010000); U24(profile, 65537);
  for (unsigned i = 0; i < 13; ++i) U16(profile, 1);
  auto* maxp = new ots::OpenTypeMAXP(&font, OTS_TAG_MAXP_EXT);
  ASSERT_TRUE(maxp->Parse(profile.data(), profile.size()));
  EXPECT_EQ(65537u, maxp->num_glyphs);
  Add(maxp);
  EXPECT_EQ(maxp, font.GetMaxp());
  ots::ExpandingMemoryStream out(64, 1024);
  ASSERT_TRUE(maxp->Serialize(&out));
  EXPECT_EQ(profile.size(), static_cast<size_t>(out.Tell()));
  EXPECT_EQ(0, memcmp(profile.data(), out.get(), profile.size()));

  Bytes header(34, 0);
  Set32(header, 0, 0x00010000); U32(header, 65537);
  auto* hhea = new ots::OpenTypeHHEA(&font, OTS_TAG_HHEA_EXT);
  ASSERT_TRUE(hhea->Parse(header.data(), header.size()));
  EXPECT_EQ(65537u, hhea->num_metrics);
  Add(hhea);
  Bytes metrics(65537 * 4, 0);
  ots::OpenTypeHMTX hmtx(&font, OTS_TAG_HMTX_EXT);
  EXPECT_TRUE(hmtx.Parse(metrics.data(), metrics.size()));
  metrics.pop_back();
  ots::OpenTypeHMTX truncated(&font, OTS_TAG_HMTX_EXT);
  EXPECT_FALSE(truncated.Parse(metrics.data(), metrics.size()));
}

TEST_F(OFFTest, ExtendedOutlinesAndCompositeGlyphIDs) {
  auto* maxp = new ots::OpenTypeMAXP(&font, OTS_TAG_MAXP_EXT);
  maxp->num_glyphs = 65537; maxp->version_1 = false;
  Add(maxp);
  auto* head = new ots::OpenTypeHEAD(&font, OTS_TAG_HEAD);
  head->index_to_loc_format = 1;
  Add(head);
  Add(new ots::OpenTypeNAME(&font, OTS_TAG_NAME));
  Bytes offsets;
  // Glyph zero is a composite of glyph 65536, a simple cubic outline.
  U32(offsets, 0);
  for (unsigned i = 1; i <= 65536; ++i) U32(offsets, 20);
  U32(offsets, 40);
  auto* loca = new ots::OpenTypeLOCA(&font, OTS_TAG_LOCA_EXT);
  ASSERT_TRUE(loca->Parse(offsets.data(), offsets.size()));
  Add(loca);
  Bytes outlines;
  U16(outlines, 0xffff);
  for (unsigned i = 0; i < 4; ++i) U16(outlines, 0);
  U16(outlines, 0x2002); U24(outlines, 65536);
  U16(outlines, 0); outlines.resize(20);
  U16(outlines, 1);
  for (unsigned i = 0; i < 4; ++i) U16(outlines, 0);
  U16(outlines, 3); U16(outlines, 0);
  outlines.insert(outlines.end(), {0xb1, 0xb0, 0xb0, 0xb1});
  outlines.resize(40);
  ots::OpenTypeGLYF glyf(&font, OTS_TAG_GLYF_EXT);
  EXPECT_TRUE(glyf.Parse(outlines.data(), outlines.size()));
}

TEST_F(OFFTest, ExtendedGlyphVariations) {
  auto* maxp = new ots::OpenTypeMAXP(&font, OTS_TAG_MAXP_EXT);
  maxp->num_glyphs = 65537; maxp->version_1 = false; Add(maxp);
  font.has_varc = true;
  Bytes gvar;
  U16(gvar, 1); U16(gvar, 0); U16(gvar, 1); U16(gvar, 0);
  U32(gvar, 0); U24(gvar, 65537); U16(gvar, 0);
  U32(gvar, 21 + 65538 * 2); gvar.resize(21 + 65538 * 2);
  ots::OpenTypeGVAR table(&font, OTS_TAG_GVAR_EXT);
  EXPECT_TRUE(table.Parse(gvar.data(), gvar.size()));
  EXPECT_TRUE(table.ShouldSerialize());
  gvar.pop_back();
  ots::OpenTypeGVAR truncated(&font, OTS_TAG_GVAR_EXT);
  EXPECT_TRUE(truncated.Parse(gvar.data(), gvar.size()));
  EXPECT_FALSE(truncated.ShouldSerialize());
}

TEST_F(OFFTest, DmapAndFormat15) {
  auto* maxp = new ots::OpenTypeMAXP(&font, OTS_TAG_MAXP_EXT);
  maxp->num_glyphs = 70001; maxp->version_1 = false; Add(maxp);
  Bytes dmap;
  U16(dmap, 0); U16(dmap, 1); U16(dmap, 0); U16(dmap, 5); U32(dmap, 12);
  U16(dmap, 15); U32(dmap, 31); U32(dmap, 1);
  U24(dmap, 0xfe0f); U32(dmap, 0); U32(dmap, 21);
  U32(dmap, 1); U24(dmap, 0x41); U24(dmap, 70000);
  ots::OpenTypeCMAP table(&font, OTS_TAG_DMAP);
  ASSERT_TRUE(table.Parse(dmap.data(), dmap.size()));
  ots::ExpandingMemoryStream out(64, 1024);
  ASSERT_TRUE(table.Serialize(&out));
  EXPECT_EQ(dmap.size(), static_cast<size_t>(out.Tell()));
  EXPECT_EQ(0, memcmp(dmap.data(), out.get(), dmap.size()));
  dmap.back()++;
  ots::OpenTypeCMAP bad(&font, OTS_TAG_DMAP);
  EXPECT_FALSE(bad.Parse(dmap.data(), dmap.size()));

  Bytes cmap;
  U16(cmap, 0); U16(cmap, 1); U16(cmap, 3); U16(cmap, 10); U32(cmap, 12);
  U16(cmap, 12); U16(cmap, 0); U32(cmap, 28); U32(cmap, 0); U32(cmap, 1);
  U32(cmap, 0x41); U32(cmap, 0x41); U32(cmap, 70000);
  ots::OpenTypeCMAP base(&font, OTS_TAG_CMAP);
  ASSERT_TRUE(base.Parse(cmap.data(), cmap.size()));
  ots::ExpandingMemoryStream out_base(64, 1024);
  EXPECT_TRUE(base.Serialize(&out_base));
}

TEST_F(OFFTest, VarcIndirectVariationStore) {
  AddInternalAxis();
  Bytes store;
  U16(store, 1); U32(store, 12); U16(store, 1); U32(store, 34);
  U32(store, 1); U32(store, 8);  // Sparse region list.
  U16(store, 1); U32(store, 6);  // Sparse region axis offset.
  U16(store, 0); U16(store, 0); U16(store, 0x4000); U16(store, 0x4000);
  store.push_back(1); U16(store, 1); U16(store, 0); U32(store, 13);
  U32(store, 0);  // Padding between the header and the INDEX.
  U32(store, 1); store.insert(store.end(), {1, 1, 3, 0, 1});
  EXPECT_TRUE(ParseVarc(Varc(store)));

  Bytes bad = store;
  Set32(bad, 22, 2);  // Axis coordinates overlap the offset array.
  EXPECT_FALSE(ParseVarc(Varc(bad)));
  bad = store;
  Set32(bad, 39, 5);  // INDEX overlaps MultiItemVariationData header.
  EXPECT_FALSE(ParseVarc(Varc(bad)));
  bad = store;
  Set32(bad, 39, store.size());
  EXPECT_FALSE(ParseVarc(Varc(bad)));
  bad = store;
  Set32(bad, 16, 4);  // Region overlaps its list's offset array.
  EXPECT_FALSE(ParseVarc(Varc(bad)));
  store.pop_back();
  EXPECT_FALSE(ParseVarc(Varc(store)));
}

TEST_F(OFFTest, VarcUint32RegionCount) {
  Bytes store;
  U16(store, 1); U32(store, 8); U16(store, 0);
  U32(store, 65536);
  for (unsigned i = 0; i < 65536; ++i) U32(store, 4 + 4 * 65536);
  U16(store, 0);  // All offsets share an empty region.
  EXPECT_TRUE(ParseVarc(Varc(store)));
  Set32(store, 8, 0xffffffff);
  EXPECT_FALSE(ParseVarc(Varc(store)));
}

}  // namespace
