// Copyright (c) 2022 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "colr.h"

#include "cpal.h"
#include "maxp.h"
#include "variations.h"

#include <set>

// COLR - Color Table
// http://www.microsoft.com/typography/otspec/colr.htm

#define TABLE_NAME "COLR"

namespace {

// Some typedefs so that our local variables will more closely parallel the spec.
typedef int16_t F2DOT14;  // 2.14 fixed-point
typedef int32_t Fixed;    // 16.16 fixed-point
typedef int16_t FWORD;    // A 16-bit integer value in font design units
typedef uint16_t UFWORD;
typedef uint32_t VarIdxBase;

constexpr F2DOT14 F2DOT14_one = 0x4000;

struct colrState
{
  // We track addresses of structs that we have already seen and checked,
  // because fonts may share these among multiple glyph descriptions.
  std::set<const uint8_t*> colorLines;
  std::set<const uint8_t*> varColorLines;
  std::set<const uint8_t*> affines;
  std::set<const uint8_t*> varAffines;
  std::set<const uint8_t*> paints;
  std::set<const uint8_t*> clipBoxes;

  std::set<uint16_t> baseGlyphIDs;

  uint16_t numGlyphs;  // from maxp
  uint16_t numPaletteEntries;  // from CPAL
  uint32_t numLayers;
};

// std::set<T>::contains isn't available until C++20, so we use this instead
// for better compatibility with old compilers.
template <typename T>
bool setContains(const std::set<T>& set, T item)
{
  return set.find(item) != set.end();
}

enum Extend : uint8_t
{
  EXTEND_PAD     = 0,
  EXTEND_REPEAT  = 1,
  EXTEND_REFLECT = 2,
};

bool ParseColorLine(const ots::Font* font,
                    const uint8_t* data, size_t length,
                    colrState& state, bool var)
{
  auto& set = var ? state.varColorLines : state.colorLines;
  if (setContains(set, data)) {
    return true;
  }
  set.insert(data);

  ots::Buffer subtable(data, length);

  uint8_t extend;
  uint16_t numColorStops;

  if (!subtable.ReadU8(&extend) ||
      !subtable.ReadU16(&numColorStops)) {
    return OTS_FAILURE_MSG("Failed to read [Var]ColorLine");
  }

  if (extend != EXTEND_PAD && extend != EXTEND_REPEAT && extend != EXTEND_REFLECT) {
    OTS_WARNING("Unknown color-line extend mode %u", extend);
  }

  for (auto i = 0u; i < numColorStops; ++i) {
    F2DOT14 stopOffset;
    uint16_t paletteIndex;
    F2DOT14 alpha;
    VarIdxBase varIndexBase;

    if (!subtable.ReadS16(&stopOffset) ||
        !subtable.ReadU16(&paletteIndex) ||
        !subtable.ReadS16(&alpha) ||
        (var && !subtable.ReadU32(&varIndexBase))) {
      return OTS_FAILURE_MSG("Failed to read [Var]ColorStop");
    }

    if (paletteIndex >= state.numPaletteEntries && paletteIndex != 0xffffu) {
      return OTS_FAILURE_MSG("Invalid palette index %u in color stop", paletteIndex);
    }

    if (alpha < 0 || alpha > F2DOT14_one) {
      OTS_WARNING("Alpha value outside valid range 0.0 - 1.0");
    }
  }

  return true;
}

// Composition modes
enum CompositeMode : uint8_t
{
  // Porter-Duff modes
  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators
  COMPOSITE_CLEAR          =  0,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_clear
  COMPOSITE_SRC            =  1,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_src
  COMPOSITE_DEST           =  2,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dst
  COMPOSITE_SRC_OVER       =  3,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcover
  COMPOSITE_DEST_OVER      =  4,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstover
  COMPOSITE_SRC_IN         =  5,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcin
  COMPOSITE_DEST_IN        =  6,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstin
  COMPOSITE_SRC_OUT        =  7,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcout
  COMPOSITE_DEST_OUT       =  8,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstout
  COMPOSITE_SRC_ATOP       =  9,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcatop
  COMPOSITE_DEST_ATOP      = 10,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstatop
  COMPOSITE_XOR            = 11,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_xor
  COMPOSITE_PLUS           = 12,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_plus

  // Blend modes
  // https://www.w3.org/TR/compositing-1/#blending
  COMPOSITE_SCREEN         = 13,  // https://www.w3.org/TR/compositing-1/#blendingscreen
  COMPOSITE_OVERLAY        = 14,  // https://www.w3.org/TR/compositing-1/#blendingoverlay
  COMPOSITE_DARKEN         = 15,  // https://www.w3.org/TR/compositing-1/#blendingdarken
  COMPOSITE_LIGHTEN        = 16,  // https://www.w3.org/TR/compositing-1/#blendinglighten
  COMPOSITE_COLOR_DODGE    = 17,  // https://www.w3.org/TR/compositing-1/#blendingcolordodge
  COMPOSITE_COLOR_BURN     = 18,  // https://www.w3.org/TR/compositing-1/#blendingcolorburn
  COMPOSITE_HARD_LIGHT     = 19,  // https://www.w3.org/TR/compositing-1/#blendinghardlight
  COMPOSITE_SOFT_LIGHT     = 20,  // https://www.w3.org/TR/compositing-1/#blendingsoftlight
  COMPOSITE_DIFFERENCE     = 21,  // https://www.w3.org/TR/compositing-1/#blendingdifference
  COMPOSITE_EXCLUSION      = 22,  // https://www.w3.org/TR/compositing-1/#blendingexclusion
  COMPOSITE_MULTIPLY       = 23,  // https://www.w3.org/TR/compositing-1/#blendingmultiply

  // Modes that, uniquely, do not operate on components
  // https://www.w3.org/TR/compositing-1/#blendingnonseparable
  COMPOSITE_HSL_HUE        = 24,  // https://www.w3.org/TR/compositing-1/#blendinghue
  COMPOSITE_HSL_SATURATION = 25,  // https://www.w3.org/TR/compositing-1/#blendingsaturation
  COMPOSITE_HSL_COLOR      = 26,  // https://www.w3.org/TR/compositing-1/#blendingcolor
  COMPOSITE_HSL_LUMINOSITY = 27,  // https://www.w3.org/TR/compositing-1/#blendingluminosity
};

bool ParseAffine(const ots::Font* font,
                 const uint8_t* data, size_t length,
                 colrState& state, bool var)
{
  auto& set = var ? state.varAffines : state.affines;
  if (setContains(set, data)) {
    return true;
  }
  set.insert(data);

  ots::Buffer subtable(data, length);

  Fixed xx, yx, xy, yy, dx, dy;
  VarIdxBase varIndexBase;

  if (!subtable.ReadS32(&xx) ||
      !subtable.ReadS32(&yx) ||
      !subtable.ReadS32(&xy) ||
      !subtable.ReadS32(&yy) ||
      !subtable.ReadS32(&dx) ||
      !subtable.ReadS32(&dy) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read [Var]Affine2x3");
  }

  return true;
}

// Paint-record dispatch function that reads the format byte and then dispatches
// to one of the record-specific helpers.
bool ParsePaint(const ots::Font* font, const uint8_t* data, size_t length, colrState& state);

// All these paint record parsers start with Skip(1) to ignore the format field,
// which the caller has already read in order to dispatch here.

bool ParsePaintColrLayers(const ots::Font* font,
                          const uint8_t* data, size_t length,
                          colrState& state)
{
  ots::Buffer subtable(data, length);

  uint8_t numLayers;
  uint32_t firstLayerIndex;

  if (!subtable.Skip(1) ||
      !subtable.ReadU8(&numLayers) ||
      !subtable.ReadU32(&firstLayerIndex)) {
    return OTS_FAILURE_MSG("Failed to read PaintColrLayers record");
  }

  if (uint64_t(firstLayerIndex) + numLayers > state.numLayers) {
    return OTS_FAILURE_MSG("PaintColrLayers exceeds bounds of layer list");
  }

  return true;
}

bool ParsePaintSolid(const ots::Font* font,
                     const uint8_t* data, size_t length,
                     colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint16_t paletteIndex;
  F2DOT14 alpha;

  if (!subtable.Skip(1) ||
      !subtable.ReadU16(&paletteIndex) ||
      !subtable.ReadS16(&alpha)) {
    return OTS_FAILURE_MSG("Failed to read PaintSolid");
  }

  if (paletteIndex >= state.numPaletteEntries && paletteIndex != 0xffffu) {
    return OTS_FAILURE_MSG("Invalid palette index %u PaintSolid", paletteIndex);
  }

  if (alpha < 0 || alpha > F2DOT14_one) {
    OTS_WARNING("Alpha value outside valid range 0.0 - 1.0");
  }

  if (var) {
    VarIdxBase varIndexBase;
    if (!subtable.ReadU32(&varIndexBase)) {
      return OTS_FAILURE_MSG("Failed to read PaintVarSolid");
    }
  }

  return true;
}

bool ParsePaintLinearGradient(const ots::Font* font,
                              const uint8_t* data, size_t length,
                              colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t colorLine;
  FWORD x0, y0, x1, y1, x2, y2;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&colorLine) ||
      !subtable.ReadS16(&x0) ||
      !subtable.ReadS16(&y0) ||
      !subtable.ReadS16(&x1) ||
      !subtable.ReadS16(&y1) ||
      !subtable.ReadS16(&x2) ||
      !subtable.ReadS16(&y2) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]LinearGradient");
  }

  if (colorLine >= length) {
    return OTS_FAILURE_MSG("ColorLine is out of bounds");
  }

  if (!ParseColorLine(font, data + colorLine, length - colorLine, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse [Var]ColorLine");
  }

  return true;
}

bool ParsePaintRadialGradient(const ots::Font* font,
                              const uint8_t* data, size_t length,
                              colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t colorLine;
  FWORD x0, y0;
  UFWORD radius0;
  FWORD x1, y1;
  UFWORD radius1;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&colorLine) ||
      !subtable.ReadS16(&x0) ||
      !subtable.ReadS16(&y0) ||
      !subtable.ReadU16(&radius0) ||
      !subtable.ReadS16(&x1) ||
      !subtable.ReadS16(&y1) ||
      !subtable.ReadU16(&radius1) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]RadialGradient");
  }

  if (colorLine >= length) {
    return OTS_FAILURE_MSG("ColorLine is out of bounds");
  }

  if (!ParseColorLine(font, data + colorLine, length - colorLine, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse [Var]ColorLine");
  }

  return true;
}

bool ParsePaintSweepGradient(const ots::Font* font,
                             const uint8_t* data, size_t length,
                             colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t colorLine;
  FWORD centerX, centerY;
  F2DOT14 startAngle, endAngle;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&colorLine) ||
      !subtable.ReadS16(&centerX) ||
      !subtable.ReadS16(&centerY) ||
      !subtable.ReadS16(&startAngle) ||
      !subtable.ReadS16(&endAngle) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]SweepGradient");
  }

  if (colorLine >= length) {
    return OTS_FAILURE_MSG("ColorLine is out of bounds");
  }

  if (!ParseColorLine(font, data + colorLine, length - colorLine, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse [Var]ColorLine");
  }

  return true;
}

bool ParsePaintGlyph(const ots::Font* font,
                     const uint8_t* data, size_t length,
                     colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  uint16_t glyphID;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadU16(&glyphID)) {
    return OTS_FAILURE_MSG("Failed to read PaintGlyph");
  }

  if (paintOffset >= length) {
    return OTS_FAILURE_MSG("PaintGlyph paint offset out of bounds");
  }

  if (glyphID >= state.numGlyphs) {
    return OTS_FAILURE_MSG("Glyph ID %u out of bounds", glyphID);
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for PaintGlyph");
  }

  return true;
}

bool ParsePaintColrGlyph(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state)
{
  ots::Buffer subtable(data, length);

  uint16_t glyphID;

  if (!subtable.Skip(1) ||
      !subtable.ReadU16(&glyphID)) {
    return OTS_FAILURE_MSG("Failed to read PaintColrGlyph");
  }

  if (!setContains(state.baseGlyphIDs, glyphID)) {
    return OTS_FAILURE_MSG("Glyph ID %u not found in BaseGlyphList", glyphID);
  }

  return true;
}

bool ParsePaintTransform(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  uint32_t transformOffset;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadU24(&transformOffset)) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Transform");
  }

  if (paintOffset >= length || transformOffset >= length) {
    return OTS_FAILURE_MSG("Paint or transform offset out of bounds");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Transform");
  }

  if (!ParseAffine(font, data + transformOffset, length - transformOffset, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse affine transform");
  }

  return true;
}

bool ParsePaintTranslate(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  FWORD dx, dy;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&dx) ||
      !subtable.ReadS16(&dy) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Translate");
  }

  if (paintOffset >= length) {
    return OTS_FAILURE_MSG("Paint offset out of bounds");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Translate");
  }

  return true;
}

bool ParsePaintScale(const ots::Font* font,
                     const uint8_t* data, size_t length,
                     colrState& state,
                     bool var, bool aroundCenter, bool uniform)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  F2DOT14 scaleX, scaleY;
  FWORD centerX, centerY;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&scaleX) ||
      (!uniform && !subtable.ReadS16(&scaleY)) ||
      (aroundCenter && (!subtable.ReadS16(&centerX) ||
                        !subtable.ReadS16(&centerY))) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Scale[...]");
  }

  if (paintOffset >= length) {
    return OTS_FAILURE_MSG("Paint[Var]Scale[...] paint offset out of bounds");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Scale[...]");
  }

  return true;
}

bool ParsePaintRotate(const ots::Font* font,
                      const uint8_t* data, size_t length,
                      colrState& state,
                      bool var, bool aroundCenter)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  F2DOT14 angle;
  FWORD centerX, centerY;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&angle) ||
      (aroundCenter && (!subtable.ReadS16(&centerX) ||
                        !subtable.ReadS16(&centerY))) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Rotate[...]");
  }

  if (paintOffset >= length) {
    return OTS_FAILURE_MSG("Paint[Var]Rotate[...] paint offset out of bounds");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Rotate[...]");
  }

  return true;
}

bool ParsePaintSkew(const ots::Font* font,
                    const uint8_t* data, size_t length,
                    colrState& state,
                    bool var, bool aroundCenter)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  F2DOT14 xSkewAngle, ySkewAngle;
  FWORD centerX, centerY;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&xSkewAngle) ||
      !subtable.ReadS16(&ySkewAngle) ||
      (aroundCenter && (!subtable.ReadS16(&centerX) ||
                        !subtable.ReadS16(&centerY))) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Skew[...]");
  }

  if (paintOffset >= length) {
    return OTS_FAILURE_MSG("Paint[Var]Skew[...] paint offset out of bounds");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Skew[...]");
  }

  return true;
}

bool ParsePaintComposite(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t sourcePaint;
  uint8_t compositeMode;
  uint32_t backdropPaint;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&sourcePaint) ||
      !subtable.ReadU8(&compositeMode) ||
      !subtable.ReadU24(&backdropPaint)) {
    return OTS_FAILURE_MSG("Failed to read PaintComposite");
  }
  if (compositeMode > COMPOSITE_HSL_LUMINOSITY) {
    OTS_WARNING("Unknown composite mode %u\n", compositeMode);
  }

  if (sourcePaint >= length) {
    return OTS_FAILURE_MSG("Source offset %u out of bounds", sourcePaint);
  }
  if (!ParsePaint(font, data + sourcePaint, length - sourcePaint, state)) {
    return OTS_FAILURE_MSG("Failed to parse source paint");
  }

  if (backdropPaint >= length) {
    return OTS_FAILURE_MSG("Backdrop offset %u out of bounds", backdropPaint);
  }
  if (!ParsePaint(font, data + backdropPaint, length - backdropPaint, state)) {
    return OTS_FAILURE_MSG("Failed to parse backdrop paint");
  }

  return true;
}

bool ParsePaint(const ots::Font* font,
                const uint8_t* data, size_t length,
                colrState& state)
{
  if (setContains(state.paints, data)) {
    return true;
  }
  state.paints.insert(data);

  ots::Buffer subtable(data, length);

  uint8_t format;

  if (!subtable.ReadU8(&format)) {
    return OTS_FAILURE_MSG("Failed to read paint record format");
  }

  switch (format) {
    case 1: return ParsePaintColrLayers(font, data, length, state);
    case 2: return ParsePaintSolid(font, data, length, state, false);
    case 3: return ParsePaintSolid(font, data, length, state, true);
    case 4: return ParsePaintLinearGradient(font, data, length, state, false);
    case 5: return ParsePaintLinearGradient(font, data, length, state, true);
    case 6: return ParsePaintRadialGradient(font, data, length, state, false);
    case 7: return ParsePaintRadialGradient(font, data, length, state, true);
    case 8: return ParsePaintSweepGradient(font, data, length, state, false);
    case 9: return ParsePaintSweepGradient(font, data, length, state, true);
    case 10: return ParsePaintGlyph(font, data, length, state);
    case 11: return ParsePaintColrGlyph(font, data, length, state);
    case 12: return ParsePaintTransform(font, data, length, state, false);
    case 13: return ParsePaintTransform(font, data, length, state, true);
    case 14: return ParsePaintTranslate(font, data, length, state, false);
    case 15: return ParsePaintTranslate(font, data, length, state, true);
    case 16: return ParsePaintScale(font, data, length, state, false, false, false); // Scale
    case 17: return ParsePaintScale(font, data, length, state, true, false, false); // VarScale
    case 18: return ParsePaintScale(font, data, length, state, false, true, false); // ScaleAroundCenter
    case 19: return ParsePaintScale(font, data, length, state, true, true, false); // VarScaleAroundCenter
    case 20: return ParsePaintScale(font, data, length, state, false, false, true); // ScaleUniform
    case 21: return ParsePaintScale(font, data, length, state, true, false, true); // VarScaleUniform
    case 22: return ParsePaintScale(font, data, length, state, false, true, true); // ScaleUniformAroundCenter
    case 23: return ParsePaintScale(font, data, length, state, true, true, true); // VarScaleUniformAroundCenter
    case 24: return ParsePaintRotate(font, data, length, state, false, false); // Rotate
    case 25: return ParsePaintRotate(font, data, length, state, true, false); // VarRotate
    case 26: return ParsePaintRotate(font, data, length, state, false, true); // RotateAroundCenter
    case 27: return ParsePaintRotate(font, data, length, state, true, true); // VarRotateAroundCenter
    case 28: return ParsePaintSkew(font, data, length, state, false, false); // Skew
    case 29: return ParsePaintSkew(font, data, length, state, true, false); // VarSkew
    case 30: return ParsePaintSkew(font, data, length, state, false, true); // SkewAroundCenter
    case 31: return ParsePaintSkew(font, data, length, state, true, true); // VarSkewAroundCenter
    case 32: return ParsePaintComposite(font, data, length, state);
    default:
      // Clients are supposed to ignore unknown paint types.
      OTS_WARNING("Unknown paint type %u", format);
      break;
  }

  return true;
}

#pragma pack(1)
struct COLRv1
{
  // Version-0 fields
  uint16_t version;
  uint16_t numBaseGlyphRecords;
  uint32_t baseGlyphRecordsOffset;
  uint32_t layerRecordsOffset;
  uint16_t numLayerRecords;
  // Version-1 additions
  uint32_t baseGlyphListOffset;
  uint32_t layerListOffset;
  uint32_t clipListOffset;
  uint32_t varIdxMapOffset;
  uint32_t varStoreOffset;
};
#pragma pack()

bool ParseBaseGlyphRecords(const ots::Font* font,
                           const uint8_t* data, size_t length,
                           uint32_t numBaseGlyphRecords,
                           uint32_t numLayerRecords,
                           colrState& state)
{
  ots::Buffer subtable(data, length);

  int32_t prevGlyphID = -1;
  for (auto i = 0u; i < numBaseGlyphRecords; ++i) {
    uint16_t glyphID,
             firstLayerIndex,
             numLayers;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU16(&firstLayerIndex) ||
        !subtable.ReadU16(&numLayers)) {
      return OTS_FAILURE_MSG("Failed to read base glyph record");
    }

    if (glyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Base glyph record glyph ID %u out of bounds", glyphID);
    }

    if (int32_t(glyphID) <= prevGlyphID) {
      return OTS_FAILURE_MSG("Base glyph record for glyph ID %u out of order", glyphID);
    }

    if (uint32_t(firstLayerIndex) + uint32_t(numLayers) > numLayerRecords) {
      return OTS_FAILURE_MSG("Layer index out of bounds");
    }

    prevGlyphID = glyphID;
  }

  return true;
}

bool ParseLayerRecords(const ots::Font* font,
                       const uint8_t* data, size_t length,
                       uint32_t numLayerRecords,
                       colrState& state)
{
  ots::Buffer subtable(data, length);

  for (auto i = 0u; i < numLayerRecords; ++i) {
    uint16_t glyphID,
             paletteIndex;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU16(&paletteIndex)) {
      return OTS_FAILURE_MSG("Failed to read layer record");
    }

    if (glyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Layer record glyph ID %u out of bounds", glyphID);
    }

    if (paletteIndex >= state.numPaletteEntries && paletteIndex != 0xffffu) {
      return OTS_FAILURE_MSG("Invalid palette index %u in layer record", paletteIndex);
    }
  }

  return true;
}

bool ParseBaseGlyphList(const ots::Font* font,
                        const uint8_t* data, size_t length,
                        colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t numBaseGlyphPaintRecords;

  if (!subtable.ReadU32(&numBaseGlyphPaintRecords)) {
    return OTS_FAILURE_MSG("Failed to read base glyph list");
  }

  int32_t prevGlyphID = -1;
  // We loop over the list twice, first to collect all the glyph IDs present,
  // and then to check they can be parsed.
  size_t saveOffset = subtable.offset();
  for (auto i = 0u; i < numBaseGlyphPaintRecords; ++i) {
    uint16_t glyphID;
    uint32_t paintOffset;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU32(&paintOffset)) {
      return OTS_FAILURE_MSG("Failed to read base glyph list");
    }

    if (glyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Base glyph list glyph ID %u out of bounds", glyphID);
    }

    if (int32_t(glyphID) <= prevGlyphID) {
      return OTS_FAILURE_MSG("Base glyph list record for glyph ID %u out of order", glyphID);
    }

    if (!paintOffset || paintOffset >= length) {
      return OTS_FAILURE_MSG("Invalid paint offset for base glyph ID %u", glyphID);
    }

    state.baseGlyphIDs.insert(glyphID);
    prevGlyphID = glyphID;
  }

  subtable.set_offset(saveOffset);
  for (auto i = 0u; i < numBaseGlyphPaintRecords; ++i) {
    uint16_t glyphID;
    uint32_t paintOffset;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU32(&paintOffset)) {
      return OTS_FAILURE_MSG("Failed to read base glyph list");
    }

    if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
      return OTS_FAILURE_MSG("Failed to parse paint for base glyph ID %u", glyphID);
    }
  }

  return true;
}

// We call this twice: first with parsePaints = false, to just get the number of layers,
// and then with parsePaints = true to actually descend the paint graphs.
bool ParseLayerList(const ots::Font* font,
                    const uint8_t* data, size_t length,
                    colrState& state, bool parsePaints)
{
  ots::Buffer subtable(data, length);

  if (!subtable.ReadU32(&state.numLayers)) {
    return OTS_FAILURE_MSG("Failed to read layer list");
  }

  if (parsePaints) {
    for (auto i = 0u; i < state.numLayers; ++i) {
      uint32_t paintOffset;

      if (!subtable.ReadU32(&paintOffset)) {
        return OTS_FAILURE_MSG("Failed to read layer list");
      }

      if (!paintOffset || paintOffset >= length) {
        return OTS_FAILURE_MSG("Invalid paint offset in layer list");
      }

      if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
        return OTS_FAILURE_MSG("Failed to parse paint for layer record");
      }
    }
  }

  return true;
}

bool ParseClipBox(const ots::Font* font,
                  const uint8_t* data, size_t length,
                  colrState& state)
{
  if (setContains(state.clipBoxes, data)) {
    return true;
  }

  ots::Buffer subtable(data, length);

  uint8_t format;
  FWORD xMin, yMin, xMax, yMax;

  if (!subtable.ReadU8(&format) ||
      !subtable.ReadS16(&xMin) ||
      !subtable.ReadS16(&yMin) ||
      !subtable.ReadS16(&xMax) ||
      !subtable.ReadS16(&yMax)) {
    return OTS_FAILURE_MSG("Failed to read clip box");
  }

  switch (format) {
    case 1:
      break;
    case 2: {
      uint32_t varIndexBase;
      if (!subtable.ReadU32(&varIndexBase)) {
        return OTS_FAILURE_MSG("Failed to read clip box");
      }
      break;
    }
    default:
      return OTS_FAILURE_MSG("Invalid clip box format: %u", format);
  }

  if (xMin > xMax || yMin > yMax) {
    return OTS_FAILURE_MSG("Invalid clip box bounds");
  }

  state.clipBoxes.insert(data);

  return true;
}

bool ParseClipList(const ots::Font* font,
                   const uint8_t* data, size_t length,
                   colrState& state)
{
  ots::Buffer subtable(data, length);

  uint8_t format;
  uint32_t numClipRecords;

  if (!subtable.ReadU8(&format) ||
      !subtable.ReadU32(&numClipRecords)) {
    return OTS_FAILURE_MSG("Failed to read clip list");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Unknown clip list format: %u", format);
  }

  int32_t prevEndGlyphID = -1;
  for (auto i = 0u; i < numClipRecords; ++i) {
    uint16_t startGlyphID,
             endGlyphID;
    uint32_t clipBoxOffset;

    if (!subtable.ReadU16(&startGlyphID) ||
        !subtable.ReadU16(&endGlyphID) ||
        !subtable.ReadU24(&clipBoxOffset)) {
      return OTS_FAILURE_MSG("Failed to read clip list");
    }

    if (int32_t(startGlyphID) <= prevEndGlyphID ||
        endGlyphID < startGlyphID ||
        endGlyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Bad or out-of-order glyph ID range %u-%u in clip list", startGlyphID, endGlyphID);
    }

    if (clipBoxOffset >= length) {
      return OTS_FAILURE_MSG("Clip box offset out of bounds for glyphs %u-%u", startGlyphID, endGlyphID);
    }

    if (!ParseClipBox(font, data + clipBoxOffset, length - clipBoxOffset, state)) {
      return OTS_FAILURE_MSG("Failed to parse clip box for glyphs %u-%u", startGlyphID, endGlyphID);
    }

    prevEndGlyphID = endGlyphID;
  }

  return true;
}

}  // namespace

namespace ots {

bool OpenTypeCOLR::Parse(const uint8_t *data, size_t length) {
  // Parsing COLR table requires |maxp->num_glyphs| and |cpal->num_palette_entries|.
  Font *font = GetFont();
  Buffer table(data, length);

  uint32_t headerSize = offsetof(COLRv1, baseGlyphListOffset);

  // Version 0 header fields.
  uint16_t version = 0;
  uint16_t numBaseGlyphRecords = 0;
  uint32_t offsetBaseGlyphRecords = 0;
  uint32_t offsetLayerRecords = 0;
  uint16_t numLayerRecords = 0;
  if (!table.ReadU16(&version) ||
      !table.ReadU16(&numBaseGlyphRecords) ||
      !table.ReadU32(&offsetBaseGlyphRecords) ||
      !table.ReadU32(&offsetLayerRecords) ||
      !table.ReadU16(&numLayerRecords)) {
    return Error("Incomplete table");
  }

  if (version > 1) {
    return Error("Bad version");
  }

  // Additional header fields for Version 1.
  uint32_t offsetBaseGlyphList = 0;
  uint32_t offsetLayerList = 0;
  uint32_t offsetClipList = 0;
  uint32_t offsetVarIdxMap = 0;
  uint32_t offsetItemVariationStore = 0;

  if (version == 1) {
    if (!table.ReadU32(&offsetBaseGlyphList) ||
        !table.ReadU32(&offsetLayerList) ||
        !table.ReadU32(&offsetClipList) ||
        !table.ReadU32(&offsetVarIdxMap) ||
        !table.ReadU32(&offsetItemVariationStore)) {
      return Error("Incomplete v.1 table");
    }
    headerSize = sizeof(COLRv1);
  }

  colrState state;

  auto* maxp = static_cast<ots::OpenTypeMAXP*>(font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  state.numGlyphs = maxp->num_glyphs;

  auto *cpal = static_cast<ots::OpenTypeCPAL*>(font->GetTypedTable(OTS_TAG_CPAL));
  if (!cpal) {
    return OTS_FAILURE_MSG("Required cpal table missing");
  }
  state.numPaletteEntries = cpal->num_palette_entries;

  if (numBaseGlyphRecords) {
    if (offsetBaseGlyphRecords < headerSize || offsetBaseGlyphRecords >= length) {
      return Error("Bad base glyph records offset in table header");
    }
    if (!ParseBaseGlyphRecords(font, data + offsetBaseGlyphRecords, length - offsetBaseGlyphRecords,
                               numBaseGlyphRecords, numLayerRecords, state)) {
      return Error("Failed to parse base glyph records");
    }
  }

  if (numLayerRecords) {
    if (offsetLayerRecords < headerSize || offsetLayerRecords >= length) {
      return Error("Bad layer records offset in table header");
    }
    if (!ParseLayerRecords(font, data + offsetLayerRecords, length - offsetLayerRecords, numLayerRecords,
                           state)) {
      return Error("Failed to parse layer records");
    }
  }

  // ParseBaseGlyphList will need the state.numLayers field, so set it now.
  if (offsetLayerList) {
    if (offsetLayerList < headerSize || offsetLayerList >= length) {
      return Error("Bad layer list offset in table header");
    }
    if (!ParseLayerList(font, data + offsetLayerList, length - offsetLayerList,
                        state, false)) {
      return Error("Failed to parse layer list");
    }
  } else {
    state.numLayers = 0;
  }

  if (offsetBaseGlyphList) {
    if (offsetBaseGlyphList < headerSize || offsetBaseGlyphList >= length) {
      return Error("Bad base glyph list offset in table header");
    }
    if (!ParseBaseGlyphList(font, data + offsetBaseGlyphList, length - offsetBaseGlyphList,
                            state)) {
      return Error("Failed to parse base glyph list");
    }
  }

  if (offsetLayerList) {
    if (!ParseLayerList(font, data + offsetLayerList, length - offsetLayerList,
                        state, true)) {
      return Error("Failed to parse layer list");
    }
  }

  if (offsetClipList) {
    if (offsetClipList < headerSize || offsetClipList >= length) {
      return Error("Bad clip list offset in table header");
    }
    if (!ParseClipList(font, data + offsetClipList, length - offsetClipList, state)) {
      return Error("Failed to parse clip list");
    }
  }

  if (offsetVarIdxMap) {
    if (offsetVarIdxMap < headerSize || offsetVarIdxMap >= length) {
      return Error("Bad delta set index offset in table header");
    }
    if (!ParseDeltaSetIndexMap(font, data + offsetVarIdxMap, length - offsetVarIdxMap)) {
      return Error("Failed to parse delta set index map");
    }
  }

  if (offsetItemVariationStore) {
    if (offsetItemVariationStore < headerSize || offsetItemVariationStore >= length) {
      return Error("Bad item variation store offset in table header");
    }
    if (!ParseItemVariationStore(font, data + offsetItemVariationStore, length - offsetItemVariationStore)) {
      return Error("Failed to parse item variation store");
    }
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeCOLR::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write COLR table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
