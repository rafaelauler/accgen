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
#include <locale>
#include <cassert>
#include <cctype>

using namespace backendgen;

// ==================== SDNode class members ==============================
SDNode::SDNode() {
  RefInstr = NULL;
  OpName = NULL;
  ops = NULL;
  NumOperands = 0;
  IsLiteral = false;
  IsSpecificReg = false;
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
    if (IsLiteral)
      // TODO: Calculate a meaningful index and use it to inform 
      // AssemblyWriter which string to use here.
      S << "(i32 0)" << *OpName;
    else
      S << TypeName << ":`$'" << *OpName;
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

void PatternTranslator::sortOperandsDefs(NameListType* OpNames, 
					 SemanticIterator SI) {
  const unsigned MARK_REMOVAL = 99999;
  std::list<const Tree*> Queue;
  std::vector<unsigned> OperandsIndexes;
  OperandsIndexes.reserve(OpNames->size());
  // Put into queue the top level operator of this semantic.
  Queue.push_back(SI->SemanticExpression);
  while(Queue.size() > 0) {
    const Tree* Element = Queue.front();
    Queue.pop_front();
    const Operand* Op = dynamic_cast<const Operand*>(Element);
    // If operand
    if (Op != NULL) {
      if (!Op->isSpecificReference()) {
	if (HasOperandNumber(Op)) 
	  OperandsIndexes.push_back
	    (ExtractOperandNumber(Op->getOperandName())); 		
      } else {
	// Specific references are not true operands and must be
	// removed from the list
	OperandsIndexes.push_back(MARK_REMOVAL);
      }
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
  // Remove requested nodes
  unsigned E = OperandsIndexes.size(), I = 0;
  assert(E == OpNames->size() && "OperandsDefs inconsistency");
  for (NameListType::iterator NI = OpNames->begin(), NE = OpNames->end();
       NI != NE;) {
    if (OperandsIndexes[I] == MARK_REMOVAL) {      
      for (unsigned I2 = I; I2 != E - 1; ++I2) {
	OperandsIndexes[I2] = OperandsIndexes[I2+1];
      }
      OperandsIndexes.pop_back();
      E--;
      NI = OpNames->erase(NI);
      continue;
    }
    ++NI;
    ++I;    
  }
  // bubble sort
  bool swapped;
  E = OperandsIndexes.size();
  do {
    swapped = false;    
    NameListType::iterator NI = OpNames->begin();
    for (I = 0; I < E - 1; ++I) {
      if (OperandsIndexes[I] > OperandsIndexes[I+1]) {
	unsigned uTemp = OperandsIndexes[I];
	std::string sTemp(*NI);
	OperandsIndexes[I] = OperandsIndexes[I+1];
	OperandsIndexes[I+1] = uTemp;
	NameListType::iterator Next = NI;
	++Next;
	*NI = *Next;
	++NI;
	*NI = sTemp;	
	swapped = true;
      } else {
	++NI;
      }
    }    
  } while (swapped);
}

// Translate a SearchResult with a pattern implementation to DAG-like
// syntax used when defining a "Pattern" object in XXXInstrInfo.td
// This DAG is connected by uses relations and nodes contain only "ins"
// operands.
// BUG: What to do when the root of the dag assigns to a register
// bound by VirtualToRealmap? How to handle Defs by Real registers?
SDNode* PatternTranslator::generateInsnsDAG(SearchResult *SR) {
  DefList Defs;
  SDNode *LastProcessedNode, *NoDefNode = NULL;
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
    sortOperandsDefs(OpNames, I->second);
    assert (Outs->size() <= 1 && "FIXME: Expecting only one definition");
    const Operand *DefOperand = (Outs->size() == 0)? NULL: *(Outs->begin());
    // Building DAG Node for this instruction
    SDNode* N = new SDNode();
    N->RefInstr = I->first;
    N->SetOperands(AllOps->size());
    // If this instruction does not define any register, then we may need
    // to put it as the root of the DAG.
    // FIXME: If we have two of such instructions, then we always use the last
    // one found as the root. Maybe issue an warning?
    if (Outs->size() == 0) {
      assert (NoDefNode == NULL && 
	"Can't support two instructions without Outs operands in a pattern");
      NoDefNode = N;
    }
    unsigned i = 0;
    if (Bindings != NULL) {
      assert(OpNames->size() + Bindings->size() == AllOps->size()
	     && "Inconsistency. Missing sufficient bindings?");
    } else {
      assert(OpNames->size() == AllOps->size() 
	     && "Inconsistency. Missing sufficient bindings?");
    }
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
	// We don't increment 'i' because we don't want to include this operand
	HasDef = true;
	continue;
      }
      // dummy operand?
      if (Element->getType() == 0) {	
	N->ops[i] = new SDNode();
	N->ops[i]->IsLiteral = true;
	// See if bindings list has a definition for it
	if (Bindings != NULL) {
	  for (BindingsList::const_iterator I = Bindings->begin(),
		 E = Bindings->end(); I != E; ++I) {
	    if (HasOperandNumber(I->first)) {
	      if(ExtractOperandNumber(I->first) == i + 1) {
		N->ops[i]->OpName = new std::string(I->second);
		// Here we leave N->TypeName empty, because a 
		// dummy operand does not have type.
		++i;
		break;
	      }
	    }
	  }	
	  continue;
	}
	// No bindings for this dummy
	// TODO: ABORT? Wait for side effect compensation processing?
	N->ops[i]->OpName = new std::string();
	++i;
	continue;
      }
      // Is this operand already defined?
      DefList::iterator DI = std::find_if(Defs.begin(), Defs.end(), 
					  DefineName(*NI));
      if (DI != Defs.end()) {
	N->ops[i] = DI->second;
	++NI;
	++i;
	continue;
      }
      // This operand is not defined by another node, so it must be input
      // to the expression
      assert(NI != OpNames->end() && 
	     "BUG: OperandsDefs lacks required definition");
      N->ops[i] = new SDNode();
      N->ops[i]->OpName = new std::string(*NI);
      // Test to see if this is a RegisterOperand
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(Element);
      if (RO != NULL) {
	// In case of a register operand, its type must be recorded to
	// be used in the LLVM pattern, so the LLVM knowns exactly
	// what register class needs to be used.
	N->ops[i]->TypeName = RO->getRegisterClass()->getName();
      } 
      ++NI;
      ++i;
    }
    // Correct the number of operands for actual number of operands
    N->NumOperands = i;
    LastProcessedNode = N;
    // Housekeeping
    delete AllOps;
    delete Outs;
  }
  // Now, we must return the last definition, i.e., the root of the DAG.
  if (NoDefNode != NULL)
    return NoDefNode;
  return LastProcessedNode;
}

SDNode *PatternTranslator::parseLLVMDAGString(const std::string &S) {
  unsigned dummy = 0;
  return parseLLVMDAGString(S, &dummy);
}

// Converts an LLVM DAG string (extracted from the rules file, when specifying
// an LLVM pattern) to an internal memory representation. 
// Recursive function.
SDNode *PatternTranslator::parseLLVMDAGString(const std::string &S,
					      unsigned *pos) { 
  using std::isalnum;
  using std::string;
  unsigned i = *pos;
  for (; i != S.size(); ++i) if (isalnum(S[i]) || S[i] == '(') break;  
  if (i == S.size())    
    return NULL;
  // If we reached here, we need to create a new node
  bool hasChildren = false;
  if (S[i] == '(') {
    hasChildren = true;
    ++i;
    for (; i != S.size(); ++i) if (!isspace(S[i])) break;
  }
  const unsigned begin = i;
  for (; i != S.size(); ++i) if (!isalnum(S[i])) break;  
  
  // Premature end of string
  if (i == S.size())
    throw new PTParseErrorException();  
  SDNode* N = new SDNode();
  // Determine if we have type specification (':')
  if (S[i] != ':') {    
    N->OpName = new string(S.substr(begin, i-begin));
  } else {
    N->TypeName = S.substr(begin, i-begin);    
    ++i; // Eat ':'
    if (S[i] != '$') {
      delete N;
      throw new PTParseErrorException();
    }
    ++i; // Eat '$'
    const unsigned namebegin = i;
    for (; i != S.size(); ++i) if (!isalnum(S[i])) break;  
    N->OpName = new string(S.substr(namebegin, i-namebegin));
  }
  if (!hasChildren) {
    for (; i != S.size(); ++i) if (!isspace(S[i])) break;
    if (S[i] == ',') i++;
    *pos = i;
    return N;
  }
  // Parse children
  std::list<SDNode*> children;
  while (SDNode* child = parseLLVMDAGString(S, &i)) {
    children.push_back(child);
  }
  // All children parsed, Now eat the closing parenthesis
  for (; i != S.size(); ++i) if (S[i] == ')') break;  
  // Mismatched parenthesis
  if (i == S.size()) {
    for (std::list<SDNode*>::const_iterator I = children.begin(),
	  E = children.end(); I != E; ++I) {
      delete *I;
    }
    throw new PTParseErrorException();
  }
  ++i; // Eat ')';
  *pos = i;
  N->SetOperands(children.size());
  unsigned cur = 0;
  for (std::list<SDNode*>::const_iterator I = children.begin(),
	E = children.end(); I != E; ++I) {
    N->ops[cur++] = *I;
  }
  return N;
}

// This function translates a SearchResult record to C++ code to emit
// the instructions indicated by SearchResults in the LLVM backend, helping
// to build the Code Generator.

// The num parameter is used to compose the name of the C++ function that
// will be generated.

std::string PatternTranslator::generateEmitCode(SearchResult *SR, 
						unsigned num) {
  std::stringstream SS;
  SDNode *RootNode = generateInsnsDAG(SR);
  
  // Start by returning the correct transformed node
  SS << "return CurDAG->SelectNodeTo(";
  
  // Now we need to traverse the DAG and translate it into C++ code to
  // reproduce it into the LLVM machine level IR.
  std::list<SDNode*> Queue;
  Queue.push_back(RootNode);
  while (Queue.size() > 0) {
    SDNode* Element = Queue.front();
    Queue.pop_front();
        
    if (Element->NumOperands != 0) {
      for (unsigned i = 0; i < Element->NumOperands; ++i) {
	Queue.push_back(Element->ops[i]);
      }
      
    }
  }
  
  return SS.str();
}


