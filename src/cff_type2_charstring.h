// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CFF_TYPE2_CHARSTRING_H_
#define OTS_CFF_TYPE2_CHARSTRING_H_

#include "cff.h"
#include "ots.h"

#include <map>
#include <vector>

namespace ots {

// Validates all charstrings in |char_strings_index|. Charstring is a small
// language for font hinting defined in Adobe Technical Note #5177.
// http://www.adobe.com/devnet/font/pdfs/5177.Type2.pdf
//
// The validation will fail if one of the following conditions is met:
//  1. The code uses more than 48 values of argument stack.
//  2. The code uses deeply nested subroutine calls (more than 10 levels.)
//  3. The code passes invalid number of operands to an operator.
//  4. The code calls an undefined global or local subroutine.
//  5. The code uses one of the following operators that are unlikely used in
//     an ordinary fonts, and could be dangerous: random, put, get, index, roll.
//
// Arguments:
//  global_subrs_index: Global subroutines which could be called by a charstring
//                      in |char_strings_index|.
//  fd_select: A map from glyph # to font #.
//  local_subrs_per_font: A list of Local Subrs associated with FDArrays. Can be
//                        empty.
//  local_subrs: A Local Subrs associated with Top DICT. Can be NULL.
//  cff_table: A buffer which contains actual byte code of charstring, global
//             subroutines and local subroutines.
bool ValidateType2CharStringIndex(
    const CFFIndex &char_strings_index,
    const CFFIndex &global_subrs_index,
    const std::map<uint16_t, uint8_t> &fd_select,
    const std::vector<CFFIndex *> &local_subrs_per_font,
    const CFFIndex *local_subrs,
    Buffer *cff_table);

}  // namespace ots

#endif  // OTS_CFF_TYPE2_CHARSTRING_H_
