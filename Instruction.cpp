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
#include "Support.h"
#include <algorithm>
#include <cstdlib>
#include <cassert>

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
    for (std::vector<Semantic>::iterator I = SemanticVec.begin(),
	   E = SemanticVec.end(); I != E; ++I)
      {
	if (I->SemanticExpression != NULL)
	  delete I->SemanticExpression;
	if (I->OperandsBindings != NULL)
	  delete I->OperandsBindings;
      }
    for (std::vector<InsnOperand*>::iterator I = Operands.begin(),
	   E = Operands.end(); I != E; ++I)
      {
	delete *I;       
      }
  }

  void Instruction::replaceStr(std::string &s, std::string Src, 
			       std::string New, size_t initPos) const {
    size_t pos = s.find(Src, initPos);
    if (pos == std::string::npos)
      return;
    s.erase(pos, Src.size());
    s.insert(pos, New);
  }

  typedef std::list<const Operand*>::iterator ConstOpIt;

  static const Operand DummyOperand(OperandType(0,0,0), "dummy");
  static const Operand MemRefOperand(OperandType(0,0,0), "mem");

  std::string Instruction::parseOperandsFmts() {
    std::string Result(OperandFmts);
    replaceStr(Result, "\\\\%lo(%)", "%");
    replaceStr(Result, "\\\\%hi(%)", "%");

    int DummyIndex = 1;
    std::list<const Operand*>* ListOps = getOperandsBySemantic();  
    Result.insert(0, Mnemonic);
    for (ConstOpIt I = ListOps->begin(), E = ListOps->end(); I != E; ++I) {
      std::string New = std::string("${");
      // Ignore operands not meant for assembly writing
      if (*I != &DummyOperand && 
	  !HasOperandNumber(*I))
	  continue;
      New.append((*I)->getOperandName());
      if (*I == &DummyOperand)
	New.append(getStrForInteger(DummyIndex++));
      New.append("}");
      replaceStr(Result, "%", New);
    }
    delete ListOps;
    return Result;
  }
  
  std::string Instruction::getName() const {
    return Name;
  }
  
  int Instruction::getOutSize() const {    
    // We extract this information from our semantics forest.
    for (SemanticIterator I = SemanticVec.begin(), E = SemanticVec.end();
	 I != E; ++I) {
      const Operator *OP = dynamic_cast<const Operator*>(I->SemanticExpression);
      assert (OP != NULL && "Semantics top level node must be an operator");
      // If semantics top level operator is not a transfer, then we don't
      // have outs
      if (OP->getType() != AssignOp)
	continue;
      // First operand of a transfer operator will give us the destination.
      // This destination is a instruction definition, thus must be
      // member of outs
      const Operand* Oper = dynamic_cast<const Operand*>((*OP)[0]);      
      if (Oper != NULL) {
	// Specific references are not really operands, they are more like
	// "constants". Moreover, we are only interested in registeroperands.
	if (!Oper->isSpecificReference() &&
	    dynamic_cast<const RegisterOperand*>(Oper) != NULL) {	  
	  return Oper->getSize();
	}
	continue;
      }
      // Exceptionally, we have a memory reference and thus it is not an operand
      const Operator* O = dynamic_cast<const Operator*>((*OP)[0]);
      assert (O != NULL && "Must be either operand or operator");
      assert (O->getType() == MemRefOp && "Transfer first child must be \
operand or memory reference.");     
      //Result->push_back(&MemRefOperand);
      return 0;      
    }
    return -1;
  }

  // Extract all output register operands in this instruction (outs)
  std::list<const Operand*>* Instruction::getOuts() const {
    std::list<const Operand*>* Result = new std::list<const Operand*>();
    // We extract this information from our semantics forest.
    for (SemanticIterator I = SemanticVec.begin(), E = SemanticVec.end();
	 I != E; ++I) {
      const Operator *OP = dynamic_cast<const Operator*>(I->SemanticExpression);
      assert (OP != NULL && "Semantics top level node must be an operator");
      // If semantics top level operator is not a transfer, then we don't
      // have outs
      if (OP->getType() != AssignOp)
	continue;
      // First operand of a transfer operator will give us the destination.
      // This destination is a instruction definition, thus must be
      // member of outs
      const Operand* Oper = dynamic_cast<const Operand*>((*OP)[0]);      
      if (Oper != NULL) {
	// Specific references are not really operands, they are more like
	// "constants". Moreover, we are only interested in registeroperands.
	if (!Oper->isSpecificReference() &&
	    dynamic_cast<const RegisterOperand*>(Oper) != NULL) {	  
	  Result->push_back(Oper);
	}
	continue;
      }
      // Exceptionally, we have a memory reference and thus it is not an operand
      const Operator* O = dynamic_cast<const Operator*>((*OP)[0]);
      assert (O != NULL && "Must be either operand or operator");
      assert (O->getType() == MemRefOp && "Transfer first child must be \
operand or memory reference.");     
      Result->push_back(&MemRefOperand);
    }
    return Result;
  }

  // Our binary predicate to compare two operands
  class OperandsComparator {
  public:
    
    bool operator() (const Operand* A, const Operand* B) const {
      const unsigned Anum = ExtractOperandNumber(A->getOperandName());
      const unsigned Bnum = ExtractOperandNumber(B->getOperandName());
      return Anum < Bnum;
    }
  };

  // Our binary predicate to decide if two operands are equal
  class OperandsEqual {
  public:
    bool operator() (const Operand* A, const Operand* B) const {
      const unsigned Anum = ExtractOperandNumber(A->getOperandName());
      const unsigned Bnum = ExtractOperandNumber(B->getOperandName());
      return Anum == Bnum;
    }
  };

  // Our functor (unary predicate) to decide if an element is member
  // of a predefined container
  template <template <class Element, class Alloc> class Container,
	    class Element>
  class IsMemberOf {
    Container<Element, std::allocator<Element> >* list;
  public:
    IsMemberOf(Container<Element, std::allocator<Element> >* list):
	       list(list) {}
    bool operator() (Element& A) {
      return (std::find(list->begin(), list->end(), A) != list->end());
    } 
  };

  // Extract all used operands in this instruction (ins)
  std::list<const Operand*>* Instruction::getIns() const {
    std::list<const Operand*>* Result = getOperandsBySemantic();
    // Now we remove the defined nodes from this list, leaving the
    // used (ins) operands
    std::list<const Operand*>* Outs = getOuts();
    ConstOpIt pos;
    pos = std::remove_if(Result->begin(), Result->end(),
			 IsMemberOf<std::list, const Operand*>(Outs));
    Result->erase(pos, Result->end());
    delete Outs;
    return Result;
  }

  inline unsigned CountAssemblyUsefulOperands(std::list<const Operand*>& list)
  {
      unsigned Size = 0;
      for (std::list<const Operand*>::iterator I = list.begin(), E = list.end();
	   I != E; ++I) {
	  if (*I == &DummyOperand || HasOperandNumber(*I))
	      Size++;
      }
      return Size;
  }

  // Extract all operands (leaf nodes) from this instruction semantic forest
  std::list<const Operand*>* Instruction::getOperandsBySemantic() const {
    std::list<const Operand*>* Result = new std::list<const Operand*>();
    // For each operand, tries to discover its type (find the corresponding
    // node in all semantic trees). If not found, it is never used and
    // not considered a valid operand. Thus, the generated assembly format
    // must already provide some value for this never used operand.
    for (SemanticIterator I = SemanticVec.begin(), E = SemanticVec.end();
	 I != E; ++I) {
      std::list<const Tree*> Queue;
      Queue.push_back(I->SemanticExpression);
      while (Queue.size() > 0) {
	const Tree* Element = Queue.front();
	Queue.pop_front();	
	const Operand* Op = dynamic_cast
	  <const RegisterOperand*>(Element);
	// If register operand
	if (Op != NULL && !Op->isSpecificReference()) {
	  Result->push_back(Op);	  
	  continue;
	}
	Op = dynamic_cast<const ImmediateOperand*>(Element);
	// If immediate operand
	if (Op != NULL && !Op->isSpecificReference()) {
	  Result->push_back(Op);
	}
	Op = dynamic_cast<const Operand*>(Element);
	// Other types of operands are uninteresting
	if (Op != NULL) 
	  continue;	
	
	const AssignOperator *AO = dynamic_cast<const AssignOperator*>(Element);
	// If we have an assignment operator, includes the operands contained
	// in guarded statements
	if (AO != NULL && AO->getPredicate() != NULL) {
	  Queue.push_back(AO->getPredicate()->getLHS());
	  Queue.push_back(AO->getPredicate()->getRHS());
	}
	// Otherwise we have an operator
	const Operator* O = dynamic_cast<const Operator*>(Element);
	assert (O != NULL && "Unexpected tree node type");
	for (int I = 0, E = O->getArity(); I != E; ++I) {
	  Queue.push_back((*O)[I]);
	}
      }
    }
    // Now, we should have all kinds of operands in the list, some of which,
    // are duplicated. We sort them according to their order in the assembly
    // syntax and eliminate duplicates.
    {
      // FIXME: If we need a vector to sort, always use a vector!
      std::vector<const Operand*> Temp;
      Temp.reserve(Result->size());
      std::copy(Result->begin(), Result->end(), back_inserter(Temp));
      Result->clear();
      std::sort(Temp.begin(), Temp.end(), OperandsComparator());
      std::unique(Temp.begin(), Temp.end(), OperandsEqual());
      std::copy(Temp.begin(), Temp.end(), back_inserter(*Result));
     
    }

    // For each missing operand, we insert a dummy operand just to fill
    // the position and correctly order remaining defined operands
    for (std::list<const Operand*>::iterator I = Result->begin(),
	   E = Result->end(), C = Result->end(); 
	 I !=  E && HasOperandNumber(*I); C = I++) {
      if (C == Result->end())
	continue;
      const int Diff = ExtractOperandNumber((*I)->getOperandName()) -
	ExtractOperandNumber((*C)->getOperandName()) - 1;
      if (Diff > 0) 
	Result->insert(I, Diff, &DummyOperand);
    }
    if (Result->begin() != Result->end() && 
	HasOperandNumber(*(Result->begin()))) {
      const int Diff = ExtractOperandNumber((*(Result->begin()))
					    ->getOperandName()) - 1;
      Result->insert(Result->begin(), Diff, &DummyOperand);
    }
    const int Diff = getNumOperands() - CountAssemblyUsefulOperands(*Result);
    if (Diff > 0)
      Result->insert(Result->end(), Diff, &DummyOperand);

    return Result;
  }

  // Extract all defined registers in this instruction (defs)
  std::list<const Operand*>* Instruction::getDefs() const {
    std::list<const Operand*>* Result = new std::list<const Operand*>();
    // We extract this information from our semantics forest.
    for (SemanticIterator I = SemanticVec.begin(), E = SemanticVec.end();
	 I != E; ++I) {
      const Operator *OP = dynamic_cast<const Operator*>(I->SemanticExpression);
      assert (OP != NULL && "Semantics top level node must be an operator");
      // If semantics top level operator is not a transfer, then we don't
      // have defs
      if (OP->getType() != AssignOp)
	continue;
      // First operand of a transfer operator will give us the destination.
      // This destination is a instruction definition, thus it maybe
      // have a specific reference, that is, a register definition.
      const Operand* Oper = dynamic_cast<const Operand*>((*OP)[0]);      
      if (Oper != NULL) {
	if (Oper->isSpecificReference() &&
	    dynamic_cast<const RegisterOperand*>(Oper) != NULL) {	  
	  Result->push_back(Oper);
	}
	continue;
      }
    }
    return Result;
  }

  CnstOperandsList* Instruction::getUses() const {
    CnstOperandsList* Result = new std::list<const Operand*>();
    CnstOperandsList* Defs = this->getDefs();
    IsMemberOf<std::list, const Operand*> isMemberOf(Defs);
    for (SemanticIterator I = SemanticVec.begin(), E = SemanticVec.end();
	 I != E; ++I) {
      std::list<const Tree*> Queue;
      Queue.push_back(I->SemanticExpression);
      while (Queue.size() > 0) {
	const Tree* Element = Queue.front();
	Queue.pop_front();
	const Operand* Op = dynamic_cast
	  <const RegisterOperand*>(Element);
	// If register operand and specific reference, then we
	// have a use or def case.
	if (Op != NULL && Op->isSpecificReference() 
	    && !isMemberOf(Op)) {
	  Result->push_back(Op);	  
	  continue;
	}
	Op = dynamic_cast<const Operand*>(Element);
	// Other types of operands are uninteresting
	if (Op != NULL) 
	  continue;	
	
	const AssignOperator *AO = dynamic_cast<const AssignOperator*>(Element);
	// If we have an assignment operator, includes the operands contained
	// in guarded statements
	if (AO != NULL && AO->getPredicate() != NULL) {
	  Queue.push_back(AO->getPredicate()->getLHS());
	  Queue.push_back(AO->getPredicate()->getRHS());
	}
	// Otherwise we have an operator
	const Operator* O = dynamic_cast<const Operator*>(Element);
	assert (O != NULL && "Unexpected tree node type");
	for (int I = 0, E = O->getArity(); I != E; ++I) {
	  Queue.push_back((*O)[I]);
	}
      }
    }
    delete Defs;
    return Result;
  }


  void Instruction::emitAssembly(std::list<std::string> Operands,
				 SemanticIterator SI, std::ostream& S) 
    const {
    // Discover which operands are available in the Operands string list
    // (the other operands are "don't care" values and must be processed
    // by side effect compensation algorithms)
    std::list<const Tree*> Queue;
    std::list<unsigned> OperandsIndexes;
    // First "insert" pre-defined operands bindings for this semantic
    if (SI->OperandsBindings != NULL) {
      for (BindingsList::const_iterator I = SI->OperandsBindings->begin(),
	     E = SI->OperandsBindings->end(); I != E; ++I) {
	if (HasOperandNumber(I->first)) {
	  OperandsIndexes.push_front(ExtractOperandNumber(I->first));
	  Operands.push_front(I->second);
	}
      }
    }
    // Put into queue the top level operator of this semantic.
    Queue.push_back(SI->SemanticExpression);
    while(Queue.size() > 0) {
      const Tree* Element = Queue.front();
      Queue.pop_front();
      const Operand* Op = dynamic_cast<const Operand*>(Element);
      // If operand
      if (Op != NULL) {
	if (HasOperandNumber(Op)) 
	  OperandsIndexes.push_back
	    (ExtractOperandNumber(Op->getOperandName())); 		
	continue;
      }      
      // Otherwise we have an operator
      const Operator* O = dynamic_cast<const Operator*>(Element);
      assert (O != NULL && "Unexpected tree node type");
      const AssignOperator* AO = dynamic_cast<const AssignOperator*>(Element);
      if (AO != NULL && AO->getPredicate() != NULL) {
	Queue.push_back(AO->getPredicate()->getLHS());
	Queue.push_back(AO->getPredicate()->getRHS());
      }
      for (int I = 0, E = O->getArity(); I != E; ++I) {
	Queue.push_back((*O)[I]);
      }
    }
    // Now we emit assembly with don't cares when we don't know the
    // operand value
    S << Mnemonic;
    std::string AssemblyOperands = OperandFmts;
    for (unsigned I = 0, E = getNumOperands(); I != E; ++I) {
      // Search for this operand
      std::list<std::string>::const_iterator OI = Operands.begin();
      for (std::list<unsigned>::const_iterator I2 = OperandsIndexes.begin(),
	     E2 = OperandsIndexes.end(); I2 != E2; ++I2) {
	assert(OI != Operands.end() && "Invalid operands names list");
	if (*I2 == I+1) 
	  break;	
	++OI;
      }
      std::string OperandName;
      // If not found
      if (OI == Operands.end())
	OperandName = "XXX"; // we don't known this operand's value
      else
	OperandName = *OI;
      replaceStr(AssemblyOperands, "%", OperandName);
    }
    S << AssemblyOperands;
    S << "\n";
  }
  
  void Instruction::addSemantic(Semantic S) {
      SemanticVec.push_back(S);
  }
  
  void Instruction::print(std::ostream &S) const {
    S << "Instruction \"" << Name << "\", semantic:\n";
    for (SemanticIterator I = SemanticVec.begin(), E = SemanticVec.end();
	 I != E; ++I)
      {
	S << "  ";
	I->SemanticExpression->print(S);       
	S << "\n";
      }
    S << "\n\n";
  }
  
  SemanticIterator Instruction::getBegin() const {
    return SemanticVec.begin();
  }
  
  SemanticIterator Instruction::getEnd() const {
    return SemanticVec.end();
  }
  
  // InstrManager member functions

  InstrManager::InstrManager() {
    OrderNum = 0;
  }
  
  InstrManager::~InstrManager() {
    for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	   E = Instructions.end(); I != E; ++I)
      {
	delete *I;
      }
  }
  
  void InstrManager::addInstruction (Instruction *Instr) {
    Instructions.push_back(Instr);
    Instr->OrderNum = OrderNum++;
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

    // Our binary predicate to compare two instructions
  class InstructionsComparator {
  public:
    bool operator() (const Instruction* A, const Instruction* B) const {
      if (A->getName() == B->getName())
	return A->OrderNum < B->OrderNum;
      return A->getName() < B->getName();
    }
  };

  void InstrManager::SortInstructions() {
    std::sort(Instructions.begin(), Instructions.end(), 
	      InstructionsComparator());
  }

}
