// Copyright(c) 2011 - 2021 The OTS Authors.All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ebsc.h"

#include <limits>
#include <vector>

// EBSC - Embedded Bitmap Scaling Table
// http://www.microsoft.com/typography/otspec/ebsc.htm

#define TABLE_NAME "EBSC"

namespace
{

} // namespace

namespace ots
{
    bool OpenTypeEBSC::Parse(const uint8_t *data, size_t length)
    {
        Font *font = GetFont();
        Buffer table(data, length);
        this->m_data = data;
        this->m_length = length;

        uint16_t version_major = 0, version_minor = 0;
        uint32_t num_sizes = 0;
        if (!table.ReadU16(&version_major) ||
            !table.ReadU16(&version_minor) ||
            !table.ReadU32(&num_sizes))
        {
            return OTS_FAILURE_MSG("Failed to read EBSC header");
        }
        if (version_major != 2 || version_minor != 0)
        {
            return OTS_FAILURE_MSG("Bad version");
        }
        // Just read the field since these fields could take an arbitrary values.
        // Each BitmapScale table is 28 bytes long and the num_sizes tables
        if (!table.Skip(28 * num_sizes))
        {
            return OTS_FAILURE_MSG("Could not BitmapScale Tables");
        }

        return true;
    }

    bool OpenTypeEBSC::Serialize(OTSStream *out)
    {
        if (!out->Write(this->m_data, this->m_length))
        {
            return Error("Failed to write EBSC table");
        }

        return true;
    }

} // namespace ots
#undef TABLE_NAME