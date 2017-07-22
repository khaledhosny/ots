// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GLAT_H_
#define OTS_GLAT_H_

#include <vector>

#include "ots.h"
#include "graphite.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeGLAT_Basic Interface
// -----------------------------------------------------------------------------

class OpenTypeGLAT_Basic : public Table {
 public:
  explicit OpenTypeGLAT_Basic(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  virtual bool Parse(const uint8_t* data, size_t length) = 0;
  virtual bool Serialize(OTSStream* out) = 0;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v1
// -----------------------------------------------------------------------------

class OpenTypeGLAT_v1 : public OpenTypeGLAT_Basic {
 public:
  explicit OpenTypeGLAT_v1(Font* font, uint32_t tag)
      : OpenTypeGLAT_Basic(font, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  struct GlatEntry : public TablePart<OpenTypeGLAT_v1> {
    GlatEntry(OpenTypeGLAT_v1* parent)
        : TablePart<OpenTypeGLAT_v1>(parent) { }
    bool ParsePart(Buffer& table);
    bool SerializePart(OTSStream* out) const;
    uint8_t attNum;
    uint8_t num;
    std::vector<int16_t> attributes;
  };
  uint32_t version;
  std::vector<GlatEntry> entries;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v2
// -----------------------------------------------------------------------------

class OpenTypeGLAT_v2 : public OpenTypeGLAT_Basic {
 public:
  explicit OpenTypeGLAT_v2(Font* font, uint32_t tag)
      : OpenTypeGLAT_Basic(font, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
 struct GlatEntry : public TablePart<OpenTypeGLAT_v2> {
   GlatEntry(OpenTypeGLAT_v2* parent)
      : TablePart<OpenTypeGLAT_v2>(parent) { }
   bool ParsePart(Buffer& table);
   bool SerializePart(OTSStream* out) const;
   int16_t attNum;
   int16_t num;
   std::vector<int16_t> attributes;
  };
  uint32_t version;
  std::vector<GlatEntry> entries;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT
// -----------------------------------------------------------------------------

class OpenTypeGLAT : public Table {
 public:
  explicit OpenTypeGLAT(Font* font, uint32_t tag)
      : Table(font, tag, tag), font(font), tag(tag) { }
  OpenTypeGLAT(const OpenTypeGLAT& other) = delete;
  OpenTypeGLAT& operator=(const OpenTypeGLAT& other) = delete;
  ~OpenTypeGLAT() { delete handler; }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  Font* font;
  uint32_t tag;
  OpenTypeGLAT_Basic* handler = nullptr;
};

}  // namespace ots

#endif  // OTS_GLAT_H_
