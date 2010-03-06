
#ifndef INSNFORMAT_H
#define INSNFORMAT_H

#include <string>
#include <sstream>
#include <vector>

namespace backendgen {

inline static std::string getStrForInteger(int Num) {
  std::ostringstream NumStr;
  NumStr << Num << std::flush;
  return NumStr.str();
}

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
  void setGroupId(unsigned Num) { GroupId = Num; }
  unsigned getGroupId() { return GroupId; }

private:
  const char *Name;
  unsigned SizeInBits;
  unsigned StartBitPos;
  unsigned EndBitPos;
  unsigned GroupId;
};

class FieldGroup {
private:
  std::vector<FormatField *> FF;
public:
  FieldGroup() {}
  ~FieldGroup() {}

  void addField(FormatField *F) {
    FF.push_back(F);
  }

  std::vector<FormatField *>& getFields() {
    return FF;
  }
};


class InsnFormat {
public:
  InsnFormat(const char *name, int sizeInBits) : 
    BaseLLVMFormatClassName("InstSP16"), Name(name), SizeInBits(sizeInBits) {}

  // Deallocate lists
  ~InsnFormat() {
    for (std::vector<FormatField*>::const_iterator I = Fields.begin(), 
	   E = Fields.end(); I != E; ++I) {
      delete *I;
    }
    for (std::vector<FieldGroup*>::const_iterator I = Groups.begin(), 
	   E = Groups.end(); I != E; ++I) {
      delete *I;
    }
  }

  std::string getName() {
    return std::string(Name);
  }

  std::string getName(unsigned GroupNum) {
    std::string N = std::string(Name);

    if (GroupNum != 0)
      N.append("_" + getStrForInteger(GroupNum));
    
    return N;
  }

  unsigned getSizeInBits() const { return SizeInBits; }

  int searchBackwards(unsigned LastFFIdx, unsigned FirstBitPos) const { 
    for (int e = LastFFIdx, i = 0; e != i; --e) {
      if (Fields[e]->getStartBitPos() == FirstBitPos)
        return e;
    }
    return -1; // Should never reach here!
  }

  void recognizeGroups() {
    
    int LastFFIdx = -1, FFIdxWithMinBitPos = -1;
    bool IsInGroupCtx = false;

    for (int i = 0, e = Fields.size(); i != e; ++i) {

      if (LastFFIdx < 0 && FFIdxWithMinBitPos < 0)
        LastFFIdx = FFIdxWithMinBitPos = i; 

      if (i != LastFFIdx && 
          (Fields[i]->getStartBitPos() > Fields[LastFFIdx]->getEndBitPos())) {

        IsInGroupCtx = true;

        // If the group is empty, search backwards for the first groups, 
        // mark it as the first one. Example:
        //
        //  FirstFFIdx -+   LastFFIdx    i
        //              |      |         |
        //              |      |    +----+
        //              v      v    v
        //    31-15, [ 14-4, 3-3 | 14-10, 9-3 | 14-5, 4-3 ], 2-0
        //
        // The first group we recognize is 2), we should go back and include
        // 1) as the first group, otherwise we miss it.
        if (Groups.empty()) {
          unsigned FirstFFIdx = searchBackwards(LastFFIdx, 
                                                Fields[i]->getStartBitPos());
          FieldGroup *FG = new FieldGroup();
          Groups.push_back(FG);
          for (unsigned fi = FirstFFIdx, fe = LastFFIdx+1; fi != fe; ++fi) {
            FG->addField(Fields[fi]);
            Fields[fi]->setGroupId(Groups.size());
          }
        }

        FieldGroup *FG = new FieldGroup();
        Groups.push_back(FG);
        FG->addField(Fields[i]);
        Fields[i]->setGroupId(Groups.size());
      }

      if (Fields[i]->getStartBitPos() < Fields[FFIdxWithMinBitPos]->getEndBitPos()) {
        IsInGroupCtx = false;
        FFIdxWithMinBitPos = i;
      }

      if (IsInGroupCtx) {
        Groups[Groups.size()-1]->addField(Fields[i]);
        Fields[i]->setGroupId(Groups.size());
      }

      LastFFIdx = i;
    }

  }

  void addField(FormatField *FF) {
    Fields.push_back(FF);
  }

  FormatField *getField(unsigned Num) {
    return Fields[Num];
  }

  std::vector<FormatField *>& getFields(unsigned Grp) {
    return Groups[Grp]->getFields();
  }

  std::vector<FormatField *>& getFields() {
    return Fields;
  }

  void setBaseLLVMFormatClassName(unsigned BaseNum) {
    BaseLLVMFormatClassName.append("_" + getStrForInteger(BaseNum));
  }

  std::string& getBaseLLVMFormatClassName() {
    return BaseLLVMFormatClassName;
  }

  unsigned getNumGroups() const { return Groups.size(); } 

private:
  std::vector<FieldGroup*> Groups;
  std::vector<FormatField*> Fields;
  std::string BaseLLVMFormatClassName;

  const char *Name;
  unsigned SizeInBits;
};


};

#endif
