// Copyright (c) 2011-2024 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_EBLC_H_
#define OTS_EBLC_H_

#include "ots.h"
#include <memory>
namespace ots
{

    class OpenTypeEBLC : public Table
    {
    public:
        explicit OpenTypeEBLC(Font *font, uint32_t tag)
            : Table(font, tag, tag),
              m_data(NULL),
              m_length(0)
        {
        }

        bool Parse(const uint8_t *data, size_t length);
        bool Serialize(OTSStream *out);

        // private:
        const uint8_t *m_data;
        size_t m_length;
    };

} // namespace ots

#endif // OTS_EBLC_H_