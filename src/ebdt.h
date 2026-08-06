// Copyright (c) 2011-2024 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_EBDT_H_
#define OTS_EBDT_H_

#include "ots.h"

namespace ots
{
    namespace ebdt
    {
        extern uint32_t BigGlyphMetricsSize;
        extern uint32_t SmallGlyphMetricsSize;
        struct BigGlyphMetrics
        {
            uint8_t height;
            uint8_t width;
            int8_t horiBearingX;
            int8_t horiBearingY;
            uint8_t horiAdvance;
            int8_t vertBearingX;
            int8_t vertBearingY;
            uint8_t vertAdvance;
        };

        struct SmallGlyphMetrics
        {
            uint8_t height;
            uint8_t width;
            int8_t bearingX;
            int8_t bearingY;
            uint8_t advance;
        };

        bool ParseBigGlyphMetrics(ots::Buffer &table, BigGlyphMetrics *metrics);
        bool ParseSmallGlyphMetrics(ots::Buffer &table, SmallGlyphMetrics *metrics);

    }
    class OpenTypeEBDT : public Table
    {
    public:
        explicit OpenTypeEBDT(Font *font, uint32_t tag)
            : Table(font, tag, tag),
              m_data(NULL),
              m_length(0)
        {
        }

        bool Parse(const uint8_t *data, size_t length);

        bool Serialize(OTSStream *out);

        bool ParseGlyphBitmapDataWithConstantMetrics(uint16_t image_format,
                                                     uint32_t ebdt_table_offset,
                                                     uint8_t bit_depth,
                                                     uint8_t width,
                                                     uint8_t height,
                                                     uint32_t *out_image_size);
        bool ParseGlyphBitmapDataWithVariableMetrics(uint16_t image_format,
                                                     uint32_t ebdt_table_offset,
                                                     uint8_t bit_depth,
                                                     uint32_t *out_image_size);

    private:
        const uint8_t *m_data;
        size_t m_length;
    };

} // namespace ots

#endif // OTS_EBDT_H_