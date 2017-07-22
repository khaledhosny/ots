// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "glat.h"

#include "gloc.h"
#include <list>

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v1
// -----------------------------------------------------------------------------

bool OpenTypeGLAT_v1::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  OpenTypeGLOC* gloc = static_cast<OpenTypeGLOC*>(
      GetFont()->GetTypedTable(OTS_TAG_GLOC));
  if (!gloc) {
    return Error("Required Gloc table is missing");
  }

  if (!table.ReadU32(&this->version) || this->version >> 16 != 1) {
    return Error("Failed to read version");
  }

  const std::vector<uint32_t>& locations = gloc->GetLocations();
  if (locations.empty()) {
    return Error("No locations from Gloc table");
  }
  std::list<uint32_t> unverified(locations.begin(), locations.end());
  while (table.remaining()) {
    GlatEntry entry(this);
    if (table.offset() > unverified.front()) {
      return Error("Offset check failed for a GlatEntry");
    }
    if (table.offset() == unverified.front()) {
      unverified.pop_front();
    }
    if (unverified.empty()) {
      return Error("Expected more locations");
    }
    if (!entry.ParsePart(table)) {
      return Error("Failed to read a GlatEntry");
    }
    this->entries.push_back(entry);
  }

  if (unverified.size() != 1 || unverified.front() != table.offset()) {
    return Error("%zu location(s) could not be verified", unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeGLAT_v1::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !SerializeParts(this->entries, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeGLAT_v1::GlatEntry::ParsePart(Buffer& table) {
  if (!table.ReadU8(&this->attNum)) {
    return parent->Error("GlatEntry: Failed to read attNum");
  }
  if (!table.ReadU8(&this->num)) {
    return parent->Error("GlatEntry: Failed to read num");
  }

  this->attributes.resize(this->num);
  for (unsigned i = 0; i < this->num; ++i) {
    if (!table.ReadS16(&this->attributes[i])) {
      return parent->Error("GlatEntry: Failed to read attribute %u", i);
    }
  }
  return true;
}

bool OpenTypeGLAT_v1::GlatEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteU8(this->attNum) ||
      !out->WriteU8(this->num) ||
      !SerializeParts(this->attributes, out)) {
    return parent->Error("GlatEntry: Failed to write");
  }
  return true;
}

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v2
// -----------------------------------------------------------------------------

bool OpenTypeGLAT_v2::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  OpenTypeGLOC* gloc = static_cast<OpenTypeGLOC*>(
      GetFont()->GetTypedTable(OTS_TAG_GLOC));
  if (!gloc) {
    return Error("Required Gloc table is missing");
  }

  if (!table.ReadU32(&this->version) || this->version >> 16 != 1) {
    return Error("Failed to read version");
  }

  const std::vector<uint32_t>& locations = gloc->GetLocations();
  if (locations.empty()) {
    return Error("No locations from Gloc table");
  }
  std::list<uint32_t> unverified(locations.begin(), locations.end());
  while (table.remaining()) {
    GlatEntry entry(this);
    if (table.offset() > unverified.front()) {
      return Error("Offset check failed for a GlatEntry");
    }
    if (table.offset() == unverified.front()) {
      unverified.pop_front();
    }
    if (unverified.empty()) {
      return Error("Expected more locations");
    }
    if (!entry.ParsePart(table)) {
      return Error("Failed to read a GlatEntry");
    }
    this->entries.push_back(entry);
  }

  if (unverified.size() != 1 || unverified.front() != table.offset()) {
    return Error("%zu location(s) could not be verified", unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeGLAT_v2::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !SerializeParts(this->entries, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeGLAT_v2::GlatEntry::ParsePart(Buffer& table) {
  if (!table.ReadS16(&this->attNum)) {
    return parent->Error("GlatEntry: Failed to read attNum");
  }
  if (!table.ReadS16(&this->num) || this->num < 0) {
    return parent->Error("GlatEntry: Failed to read valid num");
  }

  this->attributes.resize(this->num);
  for (unsigned i = 0; i < this->num; ++i) {
    if (!table.ReadS16(&this->attributes[i])) {
      return parent->Error("GlatEntry: Failed to read attribute %u", i);
    }
  }
  return true;
}

bool OpenTypeGLAT_v2::GlatEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteS16(this->attNum) ||
      !out->WriteS16(this->num) ||
      !SerializeParts(this->attributes, out)) {
    return parent->Error("GlatEntry: Failed to write");
  }
  return true;
}

// -----------------------------------------------------------------------------
// OpenTypeGLAT
// -----------------------------------------------------------------------------

bool OpenTypeGLAT::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  uint32_t version;
  if (!table.ReadU32(&version)) {
    return Error("Failed to read version");
  }
  switch (version >> 16) {
    case 1:
      this->handler = new OpenTypeGLAT_v1(this->font, this->tag);
      break;
    case 2:
      this->handler = new OpenTypeGLAT_v2(this->font, this->tag);
      break;
    default:
      return Error("Unsupported table version: %u", version >> 16);
  }
  return this->handler->Parse(data, length);
}

bool OpenTypeGLAT::Serialize(OTSStream* out) {
  if (!this->handler) {
    return Error("No Glat table parsed");
  }
  return this->handler->Serialize(out);
}

}  // namespace ots
