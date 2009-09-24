
#ifndef INSN_H
#define INSN_H

#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace backendgen {
class FormatField;
class InsnFormat;
class Insn;

typedef std::vector<Insn *> InsnVectTy;
typedef std::map<unsigned int, InsnVectTy> InsnIdMapTy;
typedef InsnIdMapTy::iterator InsnIdMapIter;

class InsnOperand {
public:
  InsnOperand(std::string name) : Name(name) {}
  ~InsnOperand() {}

  void addField(FormatField *F) {
    Fields.push_back(F);
  }

  std::string &getName() { return Name; }

  FormatField *getField(unsigned Num) { return Fields[Num]; }
  unsigned getNumFields() { return Fields.size(); }
  
private:
  std::string Name;
  std::vector<FormatField *> Fields;
};

class Insn {
public:
  Insn(std::string name, std::string operandFmts, InsnFormat *insnFormat) 
    : Name(name), OperandFmts(operandFmts), IF(insnFormat) {}
  ~Insn() {}

  void addOperand(InsnOperand *IO) {
    Operands.push_back(IO);
  }

  std::string &getName() { return Name; }
  InsnFormat *getFormat() { return IF; }

  InsnOperand *getOperand(unsigned Num) { return Operands[Num]; }
  unsigned getNumOperands() { return Operands.size(); }
  std::string &getOperandsFmts() { return OperandFmts; }
  std::string &getFmtStr() { return FmtStr; }

  void replaceStr(std::string &s, std::string Src, 
                  std::string New, size_t initPos = 0) {
    size_t pos = s.find(Src, initPos);
    if (pos == std::string::npos)
      return;
    s.erase(pos, Src.size());
    s.insert(pos, New);
  }

  void parseOperandsFmts() {
    replaceStr(OperandFmts, "\\\\%lo(%)", "%");
    replaceStr(OperandFmts, "\\\\%hi(%)", "%");

    unsigned OpNum = 0;
    while (OpNum != getNumOperands()) {
      std::string New = std::string("$");
      New.append(getOperand(OpNum)->getName());
      replaceStr(OperandFmts, "%", New);
      std::cout << OperandFmts << std::endl;
      OpNum++;
    }
  }

private:
  std::string Name, OperandFmts, FmtStr;
  InsnFormat *IF;
  std::vector<InsnOperand *> Operands;

};

};

#endif
