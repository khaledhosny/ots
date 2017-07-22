// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_SILF_H_
#define OTS_SILF_H_

#include <vector>

#include "ots.h"
#include "graphite.h"

namespace ots {

class OpenTypeSILF : public Table {
 public:
  explicit OpenTypeSILF(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  struct SILSub : public TablePart<OpenTypeSILF> {
    SILSub(OpenTypeSILF* parent)
        : TablePart<OpenTypeSILF>(parent), classes(parent) { }
    bool ParsePart(Buffer& table);
    bool SerializePart(OTSStream* out) const;
    struct JustificationLevel : public TablePart<OpenTypeSILF> {
      JustificationLevel(OpenTypeSILF* parent)
          : TablePart<OpenTypeSILF>(parent) { }
      bool ParsePart(Buffer& table);
      bool SerializePart(OTSStream* out) const;
      uint8_t attrStretch;
      uint8_t attrShrink;
      uint8_t attrStep;
      uint8_t attrWeight;
      uint8_t runto;
      uint8_t reserved;
      uint8_t reserved2;
      uint8_t reserved3;
    };
    struct PseudoMap : public TablePart<OpenTypeSILF> {
      PseudoMap(OpenTypeSILF* parent)
          : TablePart<OpenTypeSILF>(parent) { }
      bool ParsePart(Buffer& table);
      bool SerializePart(OTSStream* out) const;
      uint32_t unicode;
      uint16_t nPseudo;
    };
    struct ClassMap : public TablePart<OpenTypeSILF> {
      ClassMap(OpenTypeSILF* parent)
          : TablePart<OpenTypeSILF>(parent) { }
      bool ParsePart(Buffer& table);
      bool SerializePart(OTSStream* out) const;
      struct LookupClass : public TablePart<OpenTypeSILF> {
        LookupClass(OpenTypeSILF* parent)
            : TablePart<OpenTypeSILF>(parent) { }
        bool ParsePart(Buffer& table);
        bool SerializePart(OTSStream* out) const;
        struct LookupPair : public TablePart<OpenTypeSILF> {
          LookupPair(OpenTypeSILF* parent)
              : TablePart<OpenTypeSILF>(parent) { }
          bool ParsePart(Buffer& table);
          bool SerializePart(OTSStream* out) const;
          uint16_t glyphId;
          uint16_t index;
        };
        uint16_t numIDs;
        uint16_t searchRange;
        uint16_t entrySelector;
        uint16_t rangeShift;
        std::vector<LookupPair> lookups;
      };
      uint16_t numClass;
      uint16_t numLinear;
      std::vector<uint16_t> oClass;
      std::vector<uint16_t> glyphs;
      std::vector<LookupClass> lookups;
    };
    struct SILPass : public TablePart<OpenTypeSILF> {
      SILPass(OpenTypeSILF* parent)
          : TablePart<OpenTypeSILF>(parent) { }
      bool ParsePart(Buffer& table) { return false; }
      bool ParsePart(Buffer& table, const size_t SILSub_init_offset,
                                    const size_t next_pass_offset);
      bool SerializePart(OTSStream* out) const;
      struct PassRange : public TablePart<OpenTypeSILF> {
        PassRange(OpenTypeSILF* parent)
            : TablePart<OpenTypeSILF>(parent) { }
        bool ParsePart(Buffer& table);
        bool SerializePart(OTSStream* out) const;
        uint16_t firstId;
        uint16_t lastId;
        uint16_t colId;
      };
      uint8_t flags;
      uint8_t maxRuleLoop;
      uint8_t maxRuleContext;
      uint8_t maxBackup;
      uint16_t numRules;
      uint16_t fsmOffset;
      uint32_t pcCode;
      uint32_t rcCode;
      uint32_t aCode;
      uint32_t oDebug;
      uint16_t numRows;
      uint16_t numTransitional;
      uint16_t numSuccess;
      uint16_t numColumns;
      uint16_t numRange;
      uint16_t searchRange;
      uint16_t entrySelector;
      uint16_t rangeShift;
      std::vector<PassRange> ranges;
      std::vector<uint16_t> oRuleMap;
      std::vector<uint16_t> ruleMap;
      uint8_t minRulePreContext;
      uint8_t maxRulePreContext;
      std::vector<int16_t> startStates;
      std::vector<uint16_t> ruleSortKeys;
      std::vector<uint8_t> rulePreContext;
      uint8_t reserved;
      uint16_t pConstraint;
      std::vector<uint16_t> oConstraints;
      std::vector<uint16_t> oActions;
      std::vector<std::vector<uint16_t>> stateTrans;
      uint8_t reserved2;
      std::vector<uint8_t> passConstraints;
      std::vector<uint8_t> ruleConstraints;
      std::vector<uint8_t> actions;
      std::vector<uint16_t> dActions;
      std::vector<uint16_t> dStates;
      std::vector<uint16_t> dCols;
    };
    uint32_t ruleVersion;
    uint16_t passOffset;
    uint16_t pseudosOffset;
    uint16_t maxGlyphID;
    int16_t extraAscent;
    int16_t extraDescent;
    uint8_t numPasses;
    uint8_t iSubst;
    uint8_t iPos;
    uint8_t iJust;
    uint8_t iBidi;
    uint8_t flags;
    uint8_t maxPreContext;
    uint8_t maxPostContext;
    uint8_t attrPseudo;
    uint8_t attrBreakWeight;
    uint8_t attrDirectionality;
    uint8_t reserved;
    uint8_t reserved2;
    uint8_t numJLevels;
    std::vector<JustificationLevel> jLevels;
    uint16_t numLigComp;
    uint8_t numUserDefn;
    uint8_t maxCompPerLig;
    uint8_t direction;
    uint8_t reserved3;
    uint8_t reserved4;
    uint8_t reserved5;
    uint8_t reserved6;
    uint8_t numCritFeatures;
    std::vector<uint16_t> critFeatures;
    uint8_t reserved7;
    uint8_t numScriptTag;
    std::vector<uint32_t> scriptTag;
    uint16_t lbGID;
    std::vector<uint32_t> oPasses;
    uint16_t numPseudo;
    uint16_t searchPseudo;
    uint16_t pseudoSelector;
    uint16_t pseudoShift;
    std::vector<PseudoMap> pMaps;
    ClassMap classes;
    std::vector<SILPass> passes;
  };
  uint32_t version;
  uint32_t compilerVersion;
  uint16_t numSub;
  uint16_t reserved;
  std::vector<uint32_t> offset;
  std::vector<SILSub> tables;
};

}  // namespace ots

#endif  // OTS_SILF_H_