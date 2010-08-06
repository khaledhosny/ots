// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A parser for the Type 2 Charstring Format.
// http://www.adobe.com/devnet/font/pdfs/5177.Type2.pdf

#include "cff_type2_charstring.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <stack>
#include <string>
#include <utility>

namespace {

// The list of Operators. See Appendix. A in Adobe Technical Note #5177.
enum Type2CharStringOperator {
  kHStem = 1,
  kVStem = 3,
  kVMoveTo = 4,
  kRLineTo = 5,
  kHLineTo = 6,
  kVLineTo = 7,
  kRRCurveTo = 8,
  kCallSubr = 10,
  kReturn = 11,
  kEndChar = 14,
  kHStemHm = 18,
  kHintMask = 19,
  kCntrMask = 20,
  kRMoveTo = 21,
  kHMoveTo = 22,
  kVStemHm = 23,
  kRCurveLine = 24,
  kRLineCurve = 25,
  kVVCurveTo = 26,
  kHHCurveTo = 27,
  kCallGSubr = 29,
  kVHCurveTo = 30,
  kHVCurveTo = 31,
  kAnd = (12 << 8) + 3,
  kOr = (12 << 8) + 4,
  kNot = (12 << 8) + 5,
  kAbs = (12 << 8) + 9,
  kAdd = (12 << 8) + 10,
  kSub = (12 << 8) + 11,
  kDiv = (12 << 8) + 12,
  kNeg = (12 << 8) + 14,
  kEq = (12 << 8) + 15,
  kDrop = (12 << 8) + 18,
  kPut = (12 << 8) + 20,
  kGet = (12 << 8) + 21,
  kIfElse = (12 << 8) + 22,
  kRandom = (12 << 8) + 23,
  kMul = (12 << 8) + 24,
  kSqrt = (12 << 8) + 26,
  kDup = (12 << 8) + 27,
  kExch = (12 << 8) + 28,
  kIndex = (12 << 8) + 29,
  kRoll = (12 << 8) + 30,
  kHFlex = (12 << 8) + 34,
  kFlex = (12 << 8) + 35,
  kHFlex1 = (12 << 8) + 36,
  kFlex1 = (12 << 8) + 37,
};

// Type 2 Charstring Implementation Limits. See Appendix. B in Adobe Technical
// Note #5177.
const size_t kMaxCharStringLength = 65535;
const size_t kMaxArgumentStack = 48;
const size_t kMaxNumberOfStemHints = 96;
const size_t kMaxSubrNesting = 10;

bool ExecuteType2CharString(size_t call_depth,
                            const ots::CFFIndex& global_subrs_index,
                            const ots::CFFIndex& local_subrs_index,
                            ots::Buffer *cff_table,
                            ots::Buffer *char_string,
                            std::stack<int32_t> *argument_stack,
                            bool *out_found_endchar,
                            bool *out_found_width,
                            size_t *in_out_num_stems);

// Converts |op| to a string and returns it.
const char *Type2CharStringOperatorToString(Type2CharStringOperator op) {
  switch (op) {
  case kHStem:
    return "HStem";
  case kVStem:
    return "VStem";
  case kVMoveTo:
    return "VMoveTo";
  case kRLineTo:
    return "RLineTo";
  case kHLineTo:
    return "HLineTo";
  case kVLineTo:
    return "VLineTo";
  case kRRCurveTo:
    return "RRCurveTo";
  case kCallSubr:
    return "CallSubr";
  case kReturn:
    return "Return";
  case kEndChar:
    return "EndChar";
  case kHStemHm:
    return "HStemHm";
  case kHintMask:
    return "HintMask";
  case kCntrMask:
    return "CntrMask";
  case kRMoveTo:
    return "RMoveTo";
  case kHMoveTo:
    return "HMoveTo";
  case kVStemHm:
    return "VStemHm";
  case kRCurveLine:
    return "RCurveLine";
  case kRLineCurve:
    return "RLineCurve";
  case kVVCurveTo:
    return "VVCurveTo";
  case kHHCurveTo:
    return "HHCurveTo";
  case kCallGSubr:
    return "CallGSubr";
  case kVHCurveTo:
    return "VHCurveTo";
  case kHVCurveTo:
    return "HVCurveTo";
  case kAnd:
    return "And";
  case kOr:
    return "Or";
  case kNot:
    return "Not";
  case kAbs:
    return "Abs";
  case kAdd:
    return "Add";
  case kSub:
    return "Sub";
  case kDiv:
    return "Div";
  case kNeg:
    return "Neg";
  case kEq:
    return "Eq";
  case kDrop:
    return "Drop";
  case kPut:
    return "Put";
  case kGet:
    return "Get";
  case kIfElse:
    return "IfElse";
  case kRandom:
    return "Random";
  case kMul:
    return "Mul";
  case kSqrt:
    return "Sqrt";
  case kDup:
    return "Dup";
  case kExch:
    return "Exch";
  case kIndex:
    return "Index";
  case kRoll:
    return "Roll";
  case kHFlex:
    return "HFlex";
  case kFlex:
    return "Flex";
  case kHFlex1:
    return "HFlex1";
  case kFlex1:
    return "Flex1";
  }

  return "UNKNOWN";
}

// Read one or more bytes from the |char_string| buffer and stores the number
// read on |out_number|. If the number read is an operator (ex 'vstem'), sets
// true on |out_is_operator|. Returns true if the function read a number.
bool ReadNextNumberFromType2CharString(ots::Buffer *char_string,
                                       int32_t *out_number,
                                       bool *out_is_operator) {
  uint8_t v = 0;
  if (!char_string->ReadU8(&v)) {
    return OTS_FAILURE();
  }
  *out_is_operator = false;

  // The conversion algorithm is described in Adobe Technical Note #5177, page
  // 13, Table 1.
  if (v <= 11) {
    *out_number = v;
    *out_is_operator = true;
  } else if (v == 12) {
    uint16_t result = (v << 8);
    if (!char_string->ReadU8(&v)) {
      return OTS_FAILURE();
    }
    result += v;
    *out_number = result;
    *out_is_operator = true;
  } else if (v <= 27) {
    // Special handling for v==19 and v==20 are implemented in
    // ExecuteType2CharStringOperator().
    *out_number = v;
    *out_is_operator = true;
  } else if (v == 28) {
    if (!char_string->ReadU8(&v)) {
      return OTS_FAILURE();
    }
    uint16_t result = (v << 8);
    if (!char_string->ReadU8(&v)) {
      return OTS_FAILURE();
    }
    result += v;
    *out_number = result;
  } else if (v <= 31) {
    *out_number = v;
    *out_is_operator = true;
  } else if (v <= 246) {
    *out_number = static_cast<int32_t>(v) - 139;
  } else if (v <= 250) {
    uint8_t w = 0;
    if (!char_string->ReadU8(&w)) {
      return OTS_FAILURE();
    }
    *out_number = ((static_cast<int32_t>(v) - 247) * 256) +
        static_cast<int32_t>(w) + 108;
  } else if (v <= 254) {
    uint8_t w = 0;
    if (!char_string->ReadU8(&w)) {
      return OTS_FAILURE();
    }
    *out_number = -((static_cast<int32_t>(v) - 251) * 256) -
        static_cast<int32_t>(w) - 108;
  } else if (v == 255) {
    uint32_t result = 0;
    for (size_t i = 0; i < 4; ++i) {
      if (!char_string->ReadU8(&v)) {
        return OTS_FAILURE();
      }
      result = (result << 8) + v;
    }
    // TODO(yusukes): |result| should be treated as 16.16 fixed-point number
    // rather than 32bit signed int.
    *out_number = static_cast<int32_t>(result);
  } else {
    return OTS_FAILURE();
  }

  return true;
}

// Executes |op| and updates |argument_stack|. Returns true if the execution
// succeeds. If the |op| is kCallSubr or kCallGSubr, the function recursively
// calls ExecuteType2CharString() function. The arguments other than |op| and
// |argument_stack| are passed for that reason.
bool ExecuteType2CharStringOperator(int32_t op,
                                    size_t call_depth,
                                    const ots::CFFIndex& global_subrs_index,
                                    const ots::CFFIndex& local_subrs_index,
                                    ots::Buffer *cff_table,
                                    ots::Buffer *char_string,
                                    std::stack<int32_t> *argument_stack,
                                    bool *out_found_endchar,
                                    bool *in_out_found_width,
                                    size_t *in_out_num_stems) {
  // |dummy_result| should be a huge positive integer so callsubr and callgsubr
  // will fail with the dummy value.
  const int32_t dummy_result = std::numeric_limits<int>::max();
  const size_t stack_size = argument_stack->size();

  switch (op) {
  case kCallSubr:
  case kCallGSubr: {
    const ots::CFFIndex& subrs_index =
      (op == kCallSubr ? local_subrs_index : global_subrs_index);

    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    int32_t subr_number = argument_stack->top();
    argument_stack->pop();
    if (subr_number == dummy_result) {
      // For safety, we allow subr calls only with immediate subr numbers for
      // now. For example, we allow "123 callgsubr", but does not allow "100 12
      // add callgsubr". Please note that arithmetic and conditional operators
      // always push the |dummy_result| in this implementation.
      return OTS_FAILURE();
    }

    // See Adobe Technical Note #5176 (CFF), "16. Local/GlobalSubrs INDEXes."
    int32_t bias = 32768;
    if (subrs_index.count < 1240) {
      bias = 107;
    } else if (subrs_index.count < 33900) {
      bias = 1131;
    }
    subr_number += bias;

    // Sanity checks of |subr_number|.
    if (subr_number < 0) {
      return OTS_FAILURE();
    }
    if (subrs_index.offsets.size() <= static_cast<size_t>(subr_number)) {
      return OTS_FAILURE();  // The number is out-of-bounds.
    }

    // Prepare ots::Buffer where we're going to jump.
    const size_t length =
      subrs_index.offsets[subr_number + 1] - subrs_index.offsets[subr_number];
    if (length > kMaxCharStringLength) {
      return OTS_FAILURE();
    }
    const size_t offset = subrs_index.offsets[subr_number];
    cff_table->set_offset(offset);
    if (!cff_table->Skip(length)) {
      return OTS_FAILURE();
    }
    ots::Buffer char_string_to_jump(cff_table->buffer() + offset, length);

    return ExecuteType2CharString(call_depth + 1,
                                  global_subrs_index,
                                  local_subrs_index,
                                  cff_table,
                                  &char_string_to_jump,
                                  argument_stack,
                                  out_found_endchar,
                                  in_out_found_width,
                                  in_out_num_stems);
  }

  case kReturn:
    return true;

  case kEndChar:
    *out_found_endchar = true;
    *in_out_found_width = true;  // just in case.
    return true;

  case kHStem:
  case kVStem:
  case kHStemHm:
  case kVStemHm: {
    bool successful = false;
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    if ((stack_size % 2) == 0) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (((stack_size - 1) % 2) == 0)) {
      // The -1 is for "width" argument. For details, see Adobe Technical Note
      // #5177, page 16, note 4.
      successful = true;
    }
    (*in_out_num_stems) += (stack_size / 2);
    if ((*in_out_num_stems) > kMaxNumberOfStemHints) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;  // always set true since "w" might be 0 byte.
    return successful ? true : OTS_FAILURE();
  }

  case kRMoveTo: {
    bool successful = false;
    if (stack_size == 2) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (stack_size - 1 == 2)) {
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;
    return successful ? true : OTS_FAILURE();
  }

  case kVMoveTo:
  case kHMoveTo: {
    bool successful = false;
    if (stack_size == 1) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (stack_size - 1 == 1)) {
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;
    return successful ? true : OTS_FAILURE();
  }

  case kHintMask:
  case kCntrMask: {
    bool successful = false;
    if (stack_size == 0) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (stack_size == 1)) {
      // A number for "width" is found.
      successful = true;
    } else if ((!(*in_out_found_width)) ||  // in this case, any sizes are ok.
               ((stack_size % 2) == 0)) {
      // The numbers are vstem definition.
      // See Adobe Technical Note #5177, page 24, hintmask.
      (*in_out_num_stems) += (stack_size / 2);
      if ((*in_out_num_stems) > kMaxNumberOfStemHints) {
        return OTS_FAILURE();
      }
      successful = true;
    }
    if (!successful) {
       return OTS_FAILURE();
    }

    if ((*in_out_num_stems) == 0) {
      return OTS_FAILURE();
    }
    const size_t mask_bytes = (*in_out_num_stems + 7) / 8;
    if (!char_string->Skip(mask_bytes)) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;
    return true;
  }

  case kRLineTo:
    if (!(*in_out_found_width)) {
      // The first stack-clearing operator should be one of hstem, hstemhm,
      // vstem, vstemhm, cntrmask, hintmask, hmoveto, vmoveto, rmoveto, or
      // endchar. For details, see Adobe Technical Note #5177, page 16, note 4.
      return OTS_FAILURE();
    }
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    if ((stack_size % 2) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kHLineTo:
  case kVLineTo:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kRRCurveTo:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 6) {
      return OTS_FAILURE();
    }
    if ((stack_size % 6) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kRCurveLine:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 8) {
      return OTS_FAILURE();
    }
    if (((stack_size - 2) % 6) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kRLineCurve:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 8) {
      return OTS_FAILURE();
    }
    if (((stack_size - 6) % 2) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kVVCurveTo:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    if (((stack_size % 4) != 0) &&
        (((stack_size - 1) % 4) != 0)) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kHHCurveTo: {
    bool successful = false;
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    if ((stack_size % 4) == 0) {
      // {dxa dxb dyb dxc}+
      successful = true;
    } else if (((stack_size - 1) % 4) == 0) {
      // dy1? {dxa dxb dyb dxc}+
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return successful ? true : OTS_FAILURE();
  }

  case kVHCurveTo:
  case kHVCurveTo: {
    bool successful = false;
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    if (((stack_size - 4) % 8) == 0) {
      // dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}*
      successful = true;
    } else if ((stack_size >= 5) &&
               ((stack_size - 5) % 8) == 0) {
      // dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}* dxf
      successful = true;
    } else if ((stack_size >= 8) &&
               ((stack_size - 8) % 8) == 0) {
      // {dxa dxb dyb dyc dyd dxe dye dxf}+
      successful = true;
    } else if ((stack_size >= 9) &&
               ((stack_size - 9) % 8) == 0) {
      // {dxa dxb dyb dyc dyd dxe dye dxf}+ dyf?
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return successful ? true : OTS_FAILURE();
  }

  case kAnd:
  case kOr:
  case kEq:
  case kAdd:
  case kSub:
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kNot:
  case kAbs:
  case kNeg:
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kDiv:
    // TODO(yusukes): Should detect div-by-zero errors.
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kDrop:
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    return true;

  case kPut:
  case kGet:
  case kIndex:
    // For now, just call OTS_FAILURE since there is no way to check whether the
    // index argument, |i|, is out-of-bounds or not. Fortunately, no OpenType
    // fonts I have (except malicious ones!) use the operators.
    // TODO(yusukes): Implement them in a secure way.
    return OTS_FAILURE();

  case kRoll:
    // Likewise, just call OTS_FAILURE for kRoll since there is no way to check
    // whether |N| is smaller than the current stack depth or not.
    // TODO(yusukes): Implement them in a secure way.
    return OTS_FAILURE();

  case kRandom:
    // For now, we don't handle the 'random' operator since the operator makes
    // it hard to analyze hinting code statically.
    return OTS_FAILURE();

  case kIfElse:
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kMul:
    // TODO(yusukes): Should detect overflows.
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kSqrt:
    // TODO(yusukes): Should check if the argument is negative.
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kDup:
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    argument_stack->push(dummy_result);
    if (argument_stack->size() > kMaxArgumentStack) {
      return OTS_FAILURE();
    }
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kExch:
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case kHFlex:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 7) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kFlex:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 13) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kHFlex1:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 9) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case kFlex1:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 11) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;
  }

  OTS_WARNING("Undefined operator: %d (0x%x)", op, op);
  return OTS_FAILURE();
}

// Executes |char_string| and updates |argument_stack|.
//
// call_depth: The current call depth. Initial value is zero.
// global_subrs_index: Global subroutines.
// local_subrs_index: Local subroutines for the current glyph.
// cff_table: A whole CFF table which contains all global and local subroutines.
// char_string: A charstring we'll execute. |char_string| can be a main routine
//              in CharString INDEX, or a subroutine in GlobalSubr/LocalSubr.
// argument_stack: The stack which an operator in |char_string| operates.
// out_found_endchar: true is set if |char_string| contains 'endchar'.
// in_out_found_width: true is set if |char_string| contains 'width' byte (which
//                     is 0 or 1 byte.)
// in_out_num_stems: total number of hstems and vstems processed so far.
bool ExecuteType2CharString(size_t call_depth,
                            const ots::CFFIndex& global_subrs_index,
                            const ots::CFFIndex& local_subrs_index,
                            ots::Buffer *cff_table,
                            ots::Buffer *char_string,
                            std::stack<int32_t> *argument_stack,
                            bool *out_found_endchar,
                            bool *in_out_found_width,
                            size_t *in_out_num_stems) {
  if (call_depth > kMaxSubrNesting) {
    return OTS_FAILURE();
  }
  *out_found_endchar = false;

  const size_t length = char_string->length();
  while (char_string->offset() < length) {
    int32_t operator_or_operand = 0;
    bool is_operator = false;
    if (!ReadNextNumberFromType2CharString(char_string,
                                           &operator_or_operand,
                                           &is_operator)) {
      return OTS_FAILURE();
    }

    /*
      You can dump all operators and operands (except mask bytes for hintmask
      and cntrmask) by the following code:

      if (!is_operator) {
        std::fprintf(stderr, "#%d# ", operator_or_operand);
      } else {
        std::fprintf(stderr, "#%s#\n",
           Type2CharStringOperatorToString(
               Type2CharStringOperator(operator_or_operand)),
           operator_or_operand);
      }
    */

    if (!is_operator) {
      argument_stack->push(operator_or_operand);
      if (argument_stack->size() > kMaxArgumentStack) {
        return OTS_FAILURE();
      }
      continue;
    }

    // An operator is found. Execute it.
    if (!ExecuteType2CharStringOperator(operator_or_operand,
                                        call_depth,
                                        global_subrs_index,
                                        local_subrs_index,
                                        cff_table,
                                        char_string,
                                        argument_stack,
                                        out_found_endchar,
                                        in_out_found_width,
                                        in_out_num_stems)) {
      return OTS_FAILURE();
    }
    if (*out_found_endchar) {
      return true;
    }
    if (operator_or_operand == kReturn) {
      return true;
    }
  }

  // No endchar operator is found.
  return OTS_FAILURE();
}

// Selects a set of subroutings for |glyph_index| from |cff| and sets it on
// |out_local_subrs_to_use|. Returns true on success.
bool SelectLocalSubr(const std::map<uint16_t, uint8_t> &fd_select,
                     const std::vector<ots::CFFIndex *> &local_subrs_per_font,
                     const ots::CFFIndex *local_subrs,
                     uint16_t glyph_index,  // 0-origin
                     const ots::CFFIndex **out_local_subrs_to_use) {
  *out_local_subrs_to_use = NULL;

  // First, find local subrs from |local_subrs_per_font|.
  if ((fd_select.size() > 0) &&
      (!local_subrs_per_font.empty())) {
    // Look up FDArray index for the glyph.
    std::map<uint16_t, uint8_t>::const_iterator iter =
        fd_select.find(glyph_index);
    if (iter == fd_select.end()) {
      return OTS_FAILURE();
    }
    const uint8_t fd_index = iter->second;
    if (fd_index >= local_subrs_per_font.size()) {
      return OTS_FAILURE();
    }
    *out_local_subrs_to_use = local_subrs_per_font.at(fd_index);
  } else if (local_subrs) {
    // Second, try to use |local_subrs|. Most Latin fonts don't have FDSelect
    // entries. If The font has a local subrs index associated with the Top
    // DICT (not FDArrays), use it.
    *out_local_subrs_to_use = local_subrs;
  } else {
    // Just return NULL.
    *out_local_subrs_to_use = NULL;
  }

  return true;
}

}  // namespace

namespace ots {

bool ValidateType2CharStringIndex(
    const CFFIndex& char_strings_index,
    const CFFIndex& global_subrs_index,
    const std::map<uint16_t, uint8_t> &fd_select,
    const std::vector<CFFIndex *> &local_subrs_per_font,
    const CFFIndex *local_subrs,
    Buffer* cff_table) {
  if (char_strings_index.offsets.size() == 0) {
    return OTS_FAILURE();  // no charstring.
  }

  // For each glyph, validate the corresponding charstring.
  for (unsigned i = 1; i < char_strings_index.offsets.size(); ++i) {
    // Prepare a Buffer object, |char_string|, which contains the charstring
    // for the |i|-th glyph.
    const size_t length =
      char_strings_index.offsets[i] - char_strings_index.offsets[i - 1];
    if (length > kMaxCharStringLength) {
      return OTS_FAILURE();
    }
    const size_t offset = char_strings_index.offsets[i - 1];
    cff_table->set_offset(offset);
    if (!cff_table->Skip(length)) {
      return OTS_FAILURE();
    }
    Buffer char_string(cff_table->buffer() + offset, length);

    // Get a local subrs for the glyph.
    const unsigned glyph_index = i - 1;  // index in the map is 0-origin.
    const CFFIndex *local_subrs_to_use = NULL;
    if (!SelectLocalSubr(fd_select,
                         local_subrs_per_font,
                         local_subrs,
                         glyph_index,
                         &local_subrs_to_use)) {
      return OTS_FAILURE();
    }
    // If |local_subrs_to_use| is still NULL, use an empty one.
    CFFIndex default_empty_subrs;
    if (!local_subrs_to_use){
      local_subrs_to_use = &default_empty_subrs;
    }

    // Check a charstring for the |i|-th glyph.
    std::stack<int32_t> argument_stack;
    bool found_endchar = false;
    bool found_width = false;
    size_t num_stems = 0;
    if (!ExecuteType2CharString(0 /* initial call_depth is zero */,
                                global_subrs_index, *local_subrs_to_use,
                                cff_table, &char_string, &argument_stack,
                                &found_endchar, &found_width, &num_stems)) {
      return OTS_FAILURE();
    }
  }
  return true;
}

}  // namespace ots
