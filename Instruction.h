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

typedef std::vector<Semantic>::const_iterator SemanticIterator;
typedef std::list<const Operand*> CnstOperandsList;

typedef unsigned CostType;

struct InvalidIteratorPosException{};

// Class representing an instruction and its semantics.
// A semantic may comprise several expression trees, ran
// in parallel.
class Instruction {
  void processOperandFmts();
 public:     
  void addSemantic(Semantic S);
  Instruction(const std::string name, const std::string operandFmts,
	      InsnFormat *insnFormat, const std::string Mnemonic) : 
    Name(name), OperandFmts(operandFmts), Mnemonic(Mnemonic), IF(insnFormat) {
    processOperandFmts();
    OrderNum = 0;    
		hasDelaySlot = false;
  }
  void setCost(const CostType Cost) {this->Cost = Cost;}
  ~Instruction();
  std::string getName() const;
  void print(std::ostream &S) const;
  SemanticIterator getBegin() const;
  SemanticIterator getEnd() const;
  unsigned getIteratorPos(SemanticIterator it) const;
  SemanticIterator getIteratorAtPos(unsigned pos) const;
  CostType getCost() const {return Cost;}
  int getOutSize() const;
  CnstOperandsList* getOuts() const;
  CnstOperandsList* getIns() const;
  CnstOperandsList* getOperandsBySemantic() const;
  CnstOperandsList* getDefs() const;
  CnstOperandsList* getUses() const;
  
  bool isCall() const;
  bool isReturn() const;
  bool isJump() const;
  bool hasOperator(unsigned OperatorOpcode) const;

  void setLLVMName(std::string LLVMName) {this->LLVMName = LLVMName;}
  std::string getLLVMName() const {return LLVMName;}

  void addOperand(InsnOperand *IO) {
    Operands.push_back(IO);
  }

  InsnFormat *getFormat() { return IF; }
  
  InsnOperand *getOperand(unsigned Num) { return Operands[Num]; }
  unsigned getNumOperands() const { return Operands.size(); }
  std::string &getOperandsFmts() { return OperandFmts; }
  bool replaceStr(std::string &s, std::string Src, std::string New,
		  size_t initPos = 0) const;  
  std::string parseOperandsFmts();
  void emitAssembly(std::list<std::string> Operands, SemanticIterator SI,
		    std::ostream& S) const;
	void setHasDelaySlot(bool val) {
		hasDelaySlot = val;
	}
	bool HasDelaySlot() const {
		return hasDelaySlot;
	}

  unsigned OrderNum; // Order of appearance in archc isa file
 private:
  std::vector<Semantic> SemanticVec;
  const std::string Name;
  std::string LLVMName;
  CostType Cost;
	bool hasDelaySlot;
  // ArchC related information
  std::string OperandFmts; 
  const std::string Mnemonic;
  InsnFormat *IF;
  std::vector<InsnOperand *> Operands;
};

typedef std::vector<Instruction*>::const_iterator InstrIterator;


// Manages instruction instances.
class InstrManager {
 public:
  void addInstruction (Instruction *Instr);
  Instruction *getInstruction(const std::string &Name, unsigned Occurrence);
  Instruction *getInstruction(const std::string &LLVMName);
  InstrManager();
  ~InstrManager();
  void printAll(std::ostream &S);
  InstrIterator getBegin() const;
  InstrIterator getEnd() const;
  void SortInstructions();
  void SetLLVMNames();
 private:
  std::vector<Instruction*> Instructions;
  unsigned OrderNum; // Order of appearance in archc isa file for current ins
};

}
#endif
