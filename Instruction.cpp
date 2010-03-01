//===- Instruction.cpp - Implementation file              --*- C++ -*------===//
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
#include "Instruction.h"
#include "InsnFormat.h"

namespace backendgen {


  // InsnOperand member functions

  // Destructor must deallocate FormatFields
  InsnOperand::~InsnOperand() {
    for (std::vector<FormatField*>::iterator I = Fields.begin(),
	   E = Fields.end(); I != E; ++I)
      {
	delete *I;       
      }
  }

  // Instruction member functions    
  
  // Destructor must deallocate all semantic nodes (expression tree)
  // and InsnOperands
  Instruction::~Instruction() {
    for (std::vector<const Tree*>::iterator I = Semantic.begin(),
	   E = Semantic.end(); I != E; ++I)
      {
	delete *I;       
      }
    for (std::vector<InsnOperand*>::iterator I = Operands.begin(),
	   E = Operands.end(); I != E; ++I)
      {
	delete *I;       
      }
  }

  void Instruction::replaceStr(std::string &s, std::string Src, 
			       std::string New, size_t initPos) {
    size_t pos = s.find(Src, initPos);
    if (pos == std::string::npos)
      return;
    s.erase(pos, Src.size());
    s.insert(pos, New);
  }

  void Instruction::parseOperandsFmts() {
    replaceStr(OperandFmts, "\\\\%lo(%)", "%");
    replaceStr(OperandFmts, "\\\\%hi(%)", "%");
    
    unsigned OpNum = 0;
    while (OpNum != getNumOperands()) {
      std::string New = std::string("$");
      New.append(getOperand(OpNum)->getName());
      replaceStr(OperandFmts, "%", New);
      //std::cout << OperandFmts << std::endl;
      OpNum++;
    }
  }
  
  std::string Instruction::getName() const {
    return Name;
  }
  
  void Instruction::addSemantic(const Tree * Expression) {
      Semantic.push_back(Expression);
  }
  
  void Instruction::print(std::ostream &S) const {
    S << "Instruction \"" << Name << "\", semantic:\n";
    for (std::vector<const Tree*>::const_iterator I = Semantic.begin(),
	   E = Semantic.end(); I != E; ++I)
      {
	S << "  ";
	(*I)->print(S);       
	S << "\n";
      }
    S << "\n\n";
  }
  
  SemanticIterator Instruction::getBegin() const {
    return Semantic.begin();
  }
  
  SemanticIterator Instruction::getEnd() const {
    return Semantic.end();
  }
  
  // InstrManager member functions
  
  InstrManager::~InstrManager() {
    for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	   E = Instructions.end(); I != E; ++I)
      {
	delete *I;
      }
  }
  
  void InstrManager::addInstruction (Instruction *Instr) {
    Instructions.push_back(Instr);
  }
  
  Instruction *InstrManager::getInstruction(const std::string &Name,
					    unsigned Occurrence) {
    Instruction *Pointer = NULL;
    unsigned NumFound = 0;
    for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	   E = Instructions.end(); I != E; ++I)
      {
	if ((*I)->getName() == Name) {
	  ++NumFound;
	  if (NumFound == Occurrence) {
	    Pointer = *I;
	    break;
	  }
	}
      } 
    //if (Pointer == NULL) {
    //  Pointer = new Instruction(Name, Cost);
    //  Instructions.push_back(Pointer);
    //}
    return Pointer;
  }
  
  void InstrManager::printAll(std::ostream &S) {
    S << "Instruction Manager has a total of " << Instructions.size()
      << " instruction(s).\n";
    S << "==================================\n";
    for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	   E = Instructions.end(); I != E; ++I)
      {
	(*I)->print(S);	  
      }
  }
  
  InstrIterator InstrManager::getBegin() const {
    return Instructions.begin();
  }
  
  InstrIterator InstrManager::getEnd() const {
    return Instructions.end();
  }

}
