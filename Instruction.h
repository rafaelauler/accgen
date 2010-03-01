//===- Instruction.h - Implementation header              --*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// These classes represent instructions data, extracted from ArchC descriptions.
// Also, it adds extra information from the Compiler Backend Generation
// description files.
//
//===----------------------------------------------------------------------===//

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "InsnSelector/Semantic.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>

using namespace backendgen::expression;

namespace backendgen {
class FormatField;
class InsnFormat;
class Instruction;

typedef std::vector<Instruction *> InsnVectTy;
typedef std::map<unsigned int, InsnVectTy> InsnIdMapTy;
typedef InsnIdMapTy::iterator InsnIdMapIter;

class InsnOperand {
public:
  InsnOperand(std::string name) : Name(name) {}
  ~InsnOperand();

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

typedef std::vector<const Tree *>::const_iterator SemanticIterator;

typedef unsigned CostType;

// Class representing an instruction and its semantics.
// A semantic may comprise several expression trees, ran
// in parallel.
class Instruction {
 public:     
  void addSemantic(const Tree * Expression);
  Instruction(const std::string name, std::string operandFmts,
	      InsnFormat *insnFormat) : Name(name), OperandFmts(operandFmts),
					IF(insnFormat) {}
  void setCost(const CostType Cost) {this->Cost = Cost;}
  ~Instruction();
  std::string getName() const;
  void print(std::ostream &S) const;
  SemanticIterator getBegin() const;
  SemanticIterator getEnd() const;
  CostType getCost() const {return Cost;}

  void addOperand(InsnOperand *IO) {
    Operands.push_back(IO);
  }

  InsnFormat *getFormat() { return IF; }
  
  InsnOperand *getOperand(unsigned Num) { return Operands[Num]; }
  unsigned getNumOperands() { return Operands.size(); }
  std::string &getOperandsFmts() { return OperandFmts; }
  std::string &getFmtStr() { return FmtStr; }
  void replaceStr(std::string &s, std::string Src, std::string New,
		  size_t initPos = 0);  
  void parseOperandsFmts();

 private:
  std::vector<const Tree *> Semantic;
  const std::string Name;
  CostType Cost;
  // ArchC related information
  std::string OperandFmts, FmtStr;
  InsnFormat *IF;
  std::vector<InsnOperand *> Operands;
};

typedef std::vector<Instruction*>::const_iterator InstrIterator;


// Manages instruction instances.
class InstrManager {
 public:
  void addInstruction (Instruction *Instr);
  Instruction *getInstruction(const std::string &Name, unsigned Occurrence);
  ~InstrManager();
  void printAll(std::ostream &S);
  InstrIterator getBegin() const;
  InstrIterator getEnd() const;
 private:
  std::vector<Instruction*> Instructions;
};

}
#endif
