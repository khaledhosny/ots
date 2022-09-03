// Copyright (c) 2022 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpal.h"
#include "name.h"

// CPAL - Color Palette Table
// http://www.microsoft.com/typography/otspec/cpal.htm

#define TABLE_NAME "CPAL"

namespace {

bool ParsePaletteTypesArray(const ots::Font* font,
                            const uint8_t* data, size_t length,
                            uint16_t numPalettes)
{
  ots::Buffer subtable(data, length);

  constexpr uint32_t USABLE_WITH_LIGHT_BACKGROUND = 0x0001;
  constexpr uint32_t USABLE_WITH_DARK_BACKGROUND = 0x0002;
  constexpr uint32_t RESERVED = ~(USABLE_WITH_LIGHT_BACKGROUND | USABLE_WITH_DARK_BACKGROUND);

  for (auto i = 0u; i < numPalettes; ++i) {
    uint32_t paletteType;
    if (!subtable.ReadU32(&paletteType)) {
      return OTS_FAILURE_MSG("Failed to read palette type for index %u", i);
    }
    if (paletteType & RESERVED) {
      // Should we treat this as failure? For now, just a warning; seems unlikely
      // to be dangerous.
      OTS_WARNING("Invalid (reserved) palette type flags %08x for index %u", paletteType, i);
    }
  }

  return true;
}

bool ParsePaletteLabelsArray(const ots::Font* font,
                             const uint8_t* data, size_t length,
                             uint16_t numPalettes)
{
  ots::Buffer subtable(data, length);

  auto* name = static_cast<ots::OpenTypeNAME*>(font->GetTypedTable(OTS_TAG_NAME));
  if (!name) {
    return OTS_FAILURE_MSG("Required name table missing");
  }

  for (auto i = 0u; i < numPalettes; ++i) {
    uint16_t nameID;
    if (!subtable.ReadU16(&nameID)) {
      return OTS_FAILURE_MSG("Failed to read palette label ID for index %u", i);
    }
    if (!name->IsValidNameId(nameID)) {
      // Should we treat this as a failure?
      OTS_WARNING("Palette %u label ID %u missing from name table", i, nameID);
    }
  }

  return true;
}

bool ParsePaletteEntryLabelsArray(const ots::Font* font,
                                  const uint8_t* data, size_t length,
                                  uint16_t numPaletteEntries)
{
  ots::Buffer subtable(data, length);

  auto* name = static_cast<ots::OpenTypeNAME*>(font->GetTypedTable(OTS_TAG_NAME));
  if (!name) {
    return OTS_FAILURE_MSG("Required name table missing");
  }

  for (auto i = 0u; i < numPaletteEntries; ++i) {
    uint16_t nameID;
    if (!subtable.ReadU16(&nameID)) {
      return OTS_FAILURE_MSG("Failed to read palette entry label ID for index %u", i);
    }
    if (!name->IsValidNameId(nameID)) {
      // Should we treat this as a failure?
      OTS_WARNING("Palette entry %u label ID %u missing from name table", i, nameID);
    }
  }

  return true;
}

}  // namespace

namespace ots {

bool OpenTypeCPAL::Parse(const uint8_t *data, size_t length) {
  Font *font = GetFont();
  Buffer table(data, length);

  // Header fields common to versions 0 and 1.
  uint16_t version;
  uint16_t numPaletteEntries;
  uint16_t numPalettes;
  uint16_t numColorRecords;
  uint32_t colorRecordsArrayOffset;

  if (!table.ReadU16(&version) ||
      !table.ReadU16(&numPaletteEntries) ||
      !table.ReadU16(&numPalettes) ||
      !table.ReadU16(&numColorRecords) ||
      !table.ReadU32(&colorRecordsArrayOffset)) {
    return Error("Failed to read CPAL table header");
  }

  if (version > 1) {
    return Error("Unknown CPAL table version");
  }

  if (!numPaletteEntries || !numPalettes || !numColorRecords) {
    return Error("Empty CPAL is not valid");
  }

  uint32_t headerSize = 4 * sizeof(uint16_t) + sizeof(uint32_t) +
      numPalettes * sizeof(uint16_t);

  // uint16_t colorRecordIndices[numPalettes]
  for (auto i = 0u; i < numPalettes; ++i) {
    uint16_t colorRecordIndex;
    if (!table.ReadU16(&colorRecordIndex)) {
      return Error("Failed to read color record index");
    }
  }

  uint32_t paletteTypesArrayOffset = 0;
  uint32_t paletteLabelsArrayOffset = 0;
  uint32_t paletteEntryLabelsArrayOffset = 0;
  if (version == 1) {
    if (!table.ReadU32(&paletteTypesArrayOffset) ||
        !table.ReadU32(&paletteLabelsArrayOffset) ||
        !table.ReadU32(&paletteEntryLabelsArrayOffset)) {
      return Error("Failed to read CPAL table header");
    }
    headerSize += 3 * sizeof(uint32_t);
  }

  if (colorRecordsArrayOffset < headerSize || colorRecordsArrayOffset >= length) {
    return Error("Bad color records array offset in table header");
  }
  if (uint64_t(colorRecordsArrayOffset) + uint64_t(numColorRecords) * 4 > length) {
    return Error("Color records array exceeds table bounds");
  }

  if (paletteTypesArrayOffset) {
    if (paletteTypesArrayOffset < headerSize || paletteTypesArrayOffset >= length) {
      return Error("Bad palette types array offset in table header");
    }
    if (!ParsePaletteTypesArray(font, data + paletteTypesArrayOffset, length - paletteTypesArrayOffset,
                                numPalettes)) {
      return Error("Failed to parse palette types array");
    }
  }

  if (paletteLabelsArrayOffset) {
    if (paletteLabelsArrayOffset < headerSize || paletteLabelsArrayOffset >= length) {
      return Error("Bad palette labels array offset in table header");
    }
    if (!ParsePaletteLabelsArray(font, data + paletteLabelsArrayOffset, length - paletteLabelsArrayOffset,
                                 numPalettes)) {
      return Error("Failed to parse palette labels array");
    }
  }

  if (paletteEntryLabelsArrayOffset) {
    if (paletteEntryLabelsArrayOffset < headerSize || paletteEntryLabelsArrayOffset >= length) {
      return Error("Bad palette entry labels array offset in table header");
    }
    if (!ParsePaletteEntryLabelsArray(font, data + paletteEntryLabelsArrayOffset, length - paletteEntryLabelsArrayOffset,
                                      numPaletteEntries)) {
      return Error("Failed to parse palette entry labels array");
    }
  }

  // Record numPaletteEntries because COLR will want to validate against it.
  this->num_palette_entries = numPaletteEntries;

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeCPAL::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write CPAL table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
