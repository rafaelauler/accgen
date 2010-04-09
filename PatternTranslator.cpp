//===- PatternTranslator.cpp - ------------------------------*- C++ -*-----===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// The PatternTranslator engine translates an instruction sequence in 
// Compiler Generator's internal representation to C++ or tablegen code
// responsible for implementing the pattern.
//
//===----------------------------------------------------------------------===//

#include "PatternTranslator.h"
#include "Support.h"
#include <algorithm>
#include <cassert>

namespace backendgen {

// ==================== SDNode class members ==============================
SDNode::SDNode() {
  RefInstr = NULL;
  OpName = NULL;
  ops = NULL;
  NumOperands = 0;
}

SDNode::~SDNode() {
  if (ops != NULL) {
    assert (NumOperands > 0 && "DAG inconsistency");
    for (unsigned i = 0; i < NumOperands; ++i) {
      delete ops[i];
    }
    delete [] ops;
  } else {
    assert (NumOperands == 0 && "DAG inconsistency");
  }
  if (OpName != NULL)
    delete OpName;
}

void SDNode::SetOperands(unsigned num) {
  if (ops != NULL) {
    assert (NumOperands > 0 && "DAG inconsistency");
    for (unsigned i = 0; i < NumOperands; ++i) {
      assert (ops[i] != NULL && "DAG inconsistency");
      delete ops[i];
    }
    delete [] ops;
  }
  ops = new SDNode*[num];
  NumOperands = num;
}

// Print DAG
void SDNode::Print(std::ostream &S) {
  if (RefInstr != NULL) {
    assert (RefInstr->getLLVMName().size() > 0 && "Insns must have a LLVM name"
	    && "at this stage.");
    S << "(" << RefInstr->getLLVMName();
    if (NumOperands == 0)
      S << ")";
    else {
      for (unsigned i = 0; i < NumOperands; ++i) {
	if (i != 0)
	  S << ",";
	S << " ";
	ops[i]->Print(S);
      }
      S << ")";
    }
  } else if (OpName != NULL) {
    S << "i32:`$'" << *OpName;
  }
}

// ================ PatternTranslator class members =======================

// Private type definitions
typedef std::pair<std::string, SDNode*> Def;
typedef std::list<Def> DefList;

// Our unary predicate to decide if a DefList element is a pair defining
// "name".
class DefineName {
  const std::string &DefName;
public:
  DefineName(const std::string &DefName) : DefName(DefName) {}
  bool operator() (Def &def) const {
    if (def.first == DefName)
      return true;
    return false;
  }
};

// Translate a SearchResult with a pattern implementation to DAG-like
// syntax used when defining a "Pattern" object in XXXInstrInfo.td
// This DAG is connected by uses relations and nodes contain only "ins"
// operands.
SDNode* PatternTranslator::generateInsnsDAG(SearchResult *SR) {
  DefList Defs;
  SDNode *LastProcessedNode;
  // The idea is to discover the "outs" operands of the first instructions
  // of the sequence. This reveals us the "defs". Then, we search for uses
  // and thus "glueing" the defs as in the DAG like syntax. The last def
  // is the root of the DAG.
  OperandsDefsType::const_iterator OI = SR->OperandsDefs->begin();
  for (InstrList::const_iterator I = SR->Instructions->begin(),
	 E = SR->Instructions->end(); I != E; ++I) {
    CnstOperandsList* AllOps = I->first->getOperandsBySemantic();
    CnstOperandsList* Outs = I->first->getOuts();
    BindingsList* Bindings = I->second->OperandsBindings;
    assert (OI != SR->OperandsDefs->end() && "SearchResult inconsistency");
    NameListType* OpNames = *(OI++);
    assert (Outs->size() == 1 && "FIXME: Expecting only one definition");
    const Operand *DefOperand = *(Outs->begin());
    // Building DAG Node for this instruction
    SDNode* N = new SDNode();
    N->RefInstr = I->first;
    N->SetOperands(AllOps->size());
    unsigned i = 0;
    NameListType::const_iterator NI = OpNames->begin();
    bool HasDef = false;
    for (CnstOperandsList::const_iterator I2 = AllOps->begin(), 
	   E2 = AllOps->end(); I2 != E2; ++I2) {
      const Operand *Element = *I2;
      // Do not include outs operands in input list
      if (reinterpret_cast<const void*>(Element) == 
	  reinterpret_cast<const void*>(DefOperand)) {
	// This is the defined operand
	Defs.push_back(std::make_pair(*NI, N));
	++NI;
	++i;
	HasDef = true;
	continue;
      }
      // dummy operand?
      if (Element->getType() == 0) {
	// See if bindings list has a definition for it
	if (Bindings != NULL) {
	  for (BindingsList::const_iterator I = Bindings->begin(),
		 E = Bindings->end(); I != E; ++I) {
	    if (HasOperandNumber(I->first)) {
	      if(ExtractOperandNumber(I->first) == i + 1) {
		N->ops[i] = new SDNode();
		N->ops[i]->OpName = new std::string(I->second);
	      }
	    }
	  }	
	}
	++i;
	continue;
      }
      // Is this operands already defined?
      DefList::iterator DI = find_if(Defs.begin(), Defs.end(), 
				     DefineName(*NI));
      if (DI != Defs.end()) {
	N->ops[i] = DI->second;
      ++NI;
      ++i;
	continue;
      }
      // This operand is not defined by another node, so it must be input
      // to the expression
      N->ops[i] = new SDNode();
      N->ops[i]->OpName = new std::string(*NI);
      ++NI;
      ++i;
    }
    // Correct the number of operands for actual number of operands
    N->NumOperands = HasDef? i-1 : i;
    LastProcessedNode = N;
  }
  // Now, we must return the last definition, i.e., the root of the DAG.
  if (Defs.empty())
    return LastProcessedNode;
  return Defs.back().second;
}

} // end namespace backendgen
