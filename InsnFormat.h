
#ifndef INSNFORMAT_H
#define INSNFORMAT_H

#include <string>
#include <sstream>
#include <vector>

namespace backendgen {

  class FormatField {
  public:
    FormatField(const char *name, int sizeInBits, 
                int startBitPos, int endBitPos) : 
      Name(name), SizeInBits(sizeInBits), StartBitPos(startBitPos), 
      EndBitPos(endBitPos), GroupId(0) {}

    ~FormatField() {}
    const char *getName() const { return Name; }
    unsigned getSizeInBits() const { return SizeInBits; }
    unsigned getStartBitPos() const { return StartBitPos; }
    unsigned getEndBitPos() const { return EndBitPos; }
    void incGroupId() { GroupId++; }
    unsigned getGroupId() { return GroupId; }
  
  private:
    const char *Name;
    unsigned SizeInBits;
    unsigned StartBitPos;
    unsigned EndBitPos;
    unsigned GroupId;
  };
  
  class InsnFormat {
  public:
    InsnFormat(const char *name, int sizeInBits) : 
      Name(name), SizeInBits(sizeInBits), BaseLLVMFormatClassName("InstSP16"),
      TmpLastField(-1), NumGroups(0) {}

    ~InsnFormat() {}
    const char *getName() const { return Name; }
    unsigned getSizeInBits() const { return SizeInBits; }

    void addField(FormatField *FF) {
      Fields.push_back(FF);

      if (TmpLastField < 0)
        TmpLastField = FF->getEndBitPos();

      if (FF->getEndBitPos() > TmpLastField) {
        FF->incGroupId();
        NumGroups++;
      }

      TmpLastField = FF->getEndBitPos();
    }

    FormatField *getField(unsigned Num) {
      return Fields[Num];
    }

    std::vector<FormatField *> getFields() {
      return Fields;
    }

    void setBaseLLVMFormatClassName(unsigned BaseNum) {
      std::ostringstream NumStr;
      NumStr << BaseNum << std::flush;
      BaseLLVMFormatClassName.append("_" + NumStr.str());
    }

    std::string& getBaseLLVMFormatClassName() {
      return BaseLLVMFormatClassName;
    }

    unsigned getNumGroups() const { return NumGroups; } 
  
  private:
    std::vector<FormatField *> Fields;
    const char *Name;
    unsigned SizeInBits;
    std::string BaseLLVMFormatClassName;
    int TmpLastField;
    unsigned NumGroups;
  };


};

#endif
