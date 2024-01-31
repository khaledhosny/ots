// Copyright (c) 2011-2021 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ebdt.h"
#include <limits>
#include <vector>

#include "layout.h"
#include "maxp.h"

// EBDT - Embedded Bitmap Data Table
// http://www.microsoft.com/typography/otspec/ebdt.htm

#define TABLE_NAME "EBDT"

namespace
{

// #define DEBUG_EBDT
#ifdef DEBUG_EBDT
  std::vector<bool> formats_tested(5);
  void declare_tested_format(uint16_t index_format)
  {
    if (formats_tested[index_format])
    {
      return;
    }
    formats_tested[index_format] = true;
    printf("Tested format %d\n", index_format);
    return;
  }
#endif

  uint32_t number_of_bytes_in_bit_aligned_image_data(uint32_t width, uint32_t height, uint8_t bit_depth)
  {
    uint32_t number_of_bits = width * height * bit_depth;
    return (number_of_bits + 7) / 8;
  }
  uint32_t number_of_bytes_in_byte_aligned_image_data(uint32_t width, uint32_t height, uint8_t bit_depth)
  {
    uint32_t number_of_bits_per_row = width * bit_depth;
    uint32_t number_of_bytes_per_row = (number_of_bits_per_row + 7) / 8;
    return number_of_bytes_per_row * height;
  }

  uint32_t number_of_bytes_with_component_data(
      uint32_t numComponents)
  {
    return /* numComponents */ 2 + numComponents * (/* glyphId */ 2 + /* xOffset */ 1 + /* yOffset */ 1);
  }
} // namespace

namespace ots
{
  namespace ebdt
  {
    uint32_t BigGlyphMetricsSize = 8;
    uint32_t SmallGlyphMetricsSize = 5;
    bool ParseBigGlyphMetrics(ots::Buffer &table, BigGlyphMetrics *metrics)
    {
      if (!table.ReadU8(&metrics->height) ||
          !table.ReadU8(&metrics->width) ||
          !table.ReadS8(&metrics->horiBearingX) ||
          !table.ReadS8(&metrics->horiBearingY) ||
          !table.ReadU8(&metrics->horiAdvance) ||
          !table.ReadS8(&metrics->vertBearingX) ||
          !table.ReadS8(&metrics->vertBearingY) ||
          !table.ReadU8(&metrics->vertAdvance))
      {
        return false;
      }
      return true;
    }

    bool ParseSmallGlyphMetrics(ots::Buffer &table, SmallGlyphMetrics *metrics)
    {
      if (!table.ReadU8(&metrics->height) ||
          !table.ReadU8(&metrics->width) ||
          !table.ReadS8(&metrics->bearingX) ||
          !table.ReadS8(&metrics->bearingY) ||
          !table.ReadU8(&metrics->advance))
      {
        return false;
      }
      return true;
    }

  }
  bool OpenTypeEBDT::Parse(const uint8_t *data, size_t length)
  {
    // Font *font = GetFont();
    Buffer table(data, length);

    this->m_data = data;
    this->m_length = length;

    uint16_t version_major = 0, version_minor = 0;
    if (!table.ReadU16(&version_major) ||
        !table.ReadU16(&version_minor))
    {
      return Error("Incomplete table");
    }
    if (version_major != 2 || version_minor > 0)
    {
      return Error("Bad version");
    }
    // The rest of this table is parsed by EBLC
    return true;
  }

  bool OpenTypeEBDT::Serialize(OTSStream *out)
  {
    if (!out->Write(this->m_data, this->m_length))
    {
      return Error("Failed to write EBDT table");
    }

    return true;
  }

  bool OpenTypeEBDT::ParseGlyphBitmapDataWithVariableMetrics(uint16_t image_format,
                                                             uint32_t ebdt_table_offset,
                                                             uint8_t bit_depth,
                                                             uint32_t *out_image_size)

  {
#ifdef DEBUG_EBDT
    declare_tested_format(image_format);
#endif
    Font *font = GetFont();
    Buffer table(m_data + ebdt_table_offset, m_length - ebdt_table_offset);

    switch (image_format)
    {
    case 1:
      // small metrics, byte-aligned data
      {
        ots::ebdt::SmallGlyphMetrics metrics;
        if (!ots::ebdt::ParseSmallGlyphMetrics(table, &metrics))
        {
          return OTS_FAILURE_MSG("Failed to parse small glyph metrics");
        }
        *out_image_size = ots::ebdt::SmallGlyphMetricsSize +
                          number_of_bytes_in_byte_aligned_image_data(
                              metrics.width,
                              metrics.height,
                              bit_depth);
        if (ebdt_table_offset + *out_image_size > m_length)
        {
          return OTS_FAILURE_MSG("EBDT table too small or image size too large");
        }
        return true;
      }
    case 2:
    {
      // small metrics, bit-aligned data
      ots::ebdt::SmallGlyphMetrics metrics;
      if (!ots::ebdt::ParseSmallGlyphMetrics(table, &metrics))
      {
        return OTS_FAILURE_MSG("Failed to parse small glyph metrics");
      }
      *out_image_size = ots::ebdt::SmallGlyphMetricsSize +
                        number_of_bytes_in_bit_aligned_image_data(metrics.width,
                                                                  metrics.height,
                                                                  bit_depth);
      if (ebdt_table_offset + *out_image_size > m_length)
      {
        return OTS_FAILURE_MSG("EBDT table too small or image size too large");
      }
      return true;
    }
    case 3:
      return OTS_FAILURE_MSG("Using obsolete image format 3");
    case 4:
      return OTS_FAILURE_MSG("Using not supported image format 4");
    case 5:
      return OTS_FAILURE_MSG("Using a constant metrics image format with variable metrics");
    case 6:
      // big metrics, byte-aligned data
      {
        ots::ebdt::BigGlyphMetrics metrics;
        if (!ots::ebdt::ParseBigGlyphMetrics(table, &metrics))
        {
          return OTS_FAILURE_MSG("Failed to parse big glyph metrics");
        }
        *out_image_size = ots::ebdt::BigGlyphMetricsSize +
                          number_of_bytes_in_byte_aligned_image_data(metrics.width,
                                                                     metrics.height,
                                                                     bit_depth);
        if (ebdt_table_offset + *out_image_size > m_length)
        {
          return OTS_FAILURE_MSG("EBDT table too small or image size too large");
        }
        return true;
      }
    case 7:
      // big metrics, bit-aligned data
      {
        ots::ebdt::BigGlyphMetrics metrics;
        if (!ots::ebdt::ParseBigGlyphMetrics(table, &metrics))
        {
          return OTS_FAILURE_MSG("Failed to parse big glyph metrics");
        }
        *out_image_size = ots::ebdt::BigGlyphMetricsSize +
                          number_of_bytes_in_bit_aligned_image_data(metrics.width,
                                                                    metrics.height,
                                                                    bit_depth);
        if (ebdt_table_offset + *out_image_size > m_length)
        {
          return OTS_FAILURE_MSG("EBDT table too small or image size too large");
        }
        return true;
      }
    case 8:
      // small metrics, component data
      {
        ots::ebdt::SmallGlyphMetrics metrics;
        if (!ots::ebdt::ParseSmallGlyphMetrics(table, &metrics))
        {
          return OTS_FAILURE_MSG("Failed to parse small glyph metrics");
        }
        uint8_t pad;
        if (!table.ReadU8(&pad))
        {
          return OTS_FAILURE_MSG("Failed to read pad");
        }
        uint16_t numComponents;
        if (!table.ReadU16(&numComponents))
        {
          return OTS_FAILURE_MSG("Failed to read numComponents");
        }
        /**
         * @TODO do we need to validate that the
         * glyph Id referenced in the component is available?
         */
        *out_image_size = ots::ebdt::SmallGlyphMetricsSize + /*pad*/ 1 +
                          number_of_bytes_with_component_data(numComponents);
        if (ebdt_table_offset + *out_image_size > m_length)
        {
          return OTS_FAILURE_MSG("EBDT table too small or image size too large");
        }
      }
    case 9:
    {
      // big metrics, component data
      ots::ebdt::BigGlyphMetrics metrics;
      if (!ots::ebdt::ParseBigGlyphMetrics(table, &metrics))
      {
        return OTS_FAILURE_MSG("Failed to parse big glyph metrics");
      }

      uint16_t numComponents;
      if (!table.ReadU16(&numComponents))
      {
        return OTS_FAILURE_MSG("Failed to read numComponents");
      }
      /**
       * @TODO(mmulet) do we need to validate that the
       * glyph Id referenced in the component is available?
       */
      *out_image_size = ots::ebdt::BigGlyphMetricsSize +
                        number_of_bytes_with_component_data(numComponents);
      if (ebdt_table_offset + *out_image_size > m_length)
      {
        return OTS_FAILURE_MSG("EBDT table too small or image size too large");
      }
    }

    default:
      return Error("Unsupported image format");
    }
  }

  bool OpenTypeEBDT::ParseGlyphBitmapDataWithConstantMetrics(uint16_t image_format,
                                                             uint32_t ebdt_table_offset,
                                                             uint8_t bit_depth,
                                                             uint8_t width,
                                                             uint8_t height,
                                                             uint32_t *out_image_size)

  {
#ifdef DEBUG_EBDT
    declare_tested_format(image_format);
#endif
    Font *font = GetFont();
    // Buffer table(m_data + ebdt_table_offset, m_length = );

    switch (image_format)
    {
    case 3:
      return OTS_FAILURE_MSG("Using obsolete image format 3");
    case 4:
      return OTS_FAILURE_MSG("Using not supported image format 4");
    case 1:
    case 2:
    case 6:
    case 7:
    case 8:
    case 9:
      return OpenTypeEBDT::ParseGlyphBitmapDataWithVariableMetrics(image_format, ebdt_table_offset, bit_depth, out_image_size);
      // return OTS_FAILURE_MSG("Using a variable metrics image format with constant metrics");
    case 5:
      break;
    default:
      return OTS_FAILURE_MSG("Unsupported image format");
    }
    // Format 5: metrics in EBLC, bit-aligned image data only
    *out_image_size = number_of_bytes_in_bit_aligned_image_data(width,
                                                                height,
                                                                bit_depth);
    if (ebdt_table_offset + *out_image_size > m_length)
    {
      return OTS_FAILURE_MSG("EBDT table too small or image size too large");
    }
    return true;
  }
} // namespace ots
#undef TABLE_NAME