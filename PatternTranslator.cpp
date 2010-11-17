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
#include "LLVMDAGInfo.h"
#include "Support.h"
#include <algorithm>
#include <locale>
#include <set>
#include <cassert>
#include <cctype>

using namespace backendgen;
using LLVMDAGInfo::LLVMNodeInfoMan;
using LLVMDAGInfo::LLVMNodeInfo;
using std::string;
using std::stringstream;
using std::endl;
using std::list;    
using std::map;
using std::set;
using std::vector;
using std::pair;
using std::make_pair;

// ==================== SDNode class members ==============================
SDNode::SDNode() {
  RefInstr = NULL;
  OpName = NULL;
  ops = NULL;
  NumOperands = 0;
  LiteralIndex = 0;
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
    ops = NULL;
  }
  if (num > 0) {
    ops = new SDNode*[num];
  }
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
    if (LiteralIndex != 0)
      // Calculated a meaningful index so use it to inform 
      // AssemblyWriter which string to use here.
      S << "(i32 " << LiteralIndex << ")" << *OpName;
    else
      S << TypeName << ":`$'" << *OpName;
  }
}

// =================== MatcherCode class members ==========================
	    
void
MatcherCode::Print (std::ostream & S)
{
  S << Prolog;
  S << Code;
  S << Epilog;
}

void MatcherCode::AppendCode (const std::string& S) {
  Code.append(S);
}

// =================== LiteralMap class members ===========================

// This member function adds a new member to the string table and returns
// its index. If the string already exists, no new member is added, but a
// index to the already existing string is returned.
unsigned LiteralMap::addMember(const string& Name) {
  bool found = false;
  LiteralMap::const_iterator I, E;
  for (I = this->begin(), E = this->end(); I != E; ++I) {
    if (I->second == Name) {
      found = true;
      break;
    }    
  }
  if (found)
    return I->first;
  
  (*this)[CurrentIndex] = Name;
  return CurrentIndex++;  
}

// This member function adds a new member to the string table and returns
// its index. If the string already exists, no new member is added, but a
// index to the already existing string is returned.
unsigned LiteralMap::getMember(const string& Name) {
  bool found = false;
  LiteralMap::const_iterator I, E;
  for (I = this->begin(), E = this->end(); I != E; ++I) {
    if (I->second == Name) {
      found = true;
      break;
    }    
  }
  if (found)
    return I->first;
  
  assert (0 && "getMember failed: Member does not exist");
}

void LiteralMap::printAll(std::ostream& O) {
  for (LiteralMap::const_iterator I = this->begin(), E = this->end();
       I != E; ++I) {
    O << "  case " << I->first << ":" << endl;  
    O << "    O << \"" << I->second << "\";" << endl;
    O << "    break;" << endl;    
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
    // Depth first - use push_front
    for (int I = O->getArity() - 1, E = -1; I != E; --I) {
      Queue.push_front((*O)[I]);
    }
    const AssignOperator* AO = dynamic_cast<const AssignOperator*>(Element);
    if (AO != NULL && AO->getPredicate() != NULL) {
      Queue.push_front(AO->getPredicate()->getRHS());
      Queue.push_front(AO->getPredicate()->getLHS());      
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
SDNode* PatternTranslator::generateInsnsDAG(SearchResult *SR, 
  LiteralMap* LMap) {
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
	
	// See if bindings list has a definition for it
	if (Bindings != NULL) {
	  for (BindingsList::const_iterator I = Bindings->begin(),
		 E = Bindings->end(); I != E; ++I) {
	    if (HasOperandNumber(I->first)) {
	      if(ExtractOperandNumber(I->first) == i + 1) {
		N->ops[i]->LiteralIndex = LMap->addMember(I->second);
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

namespace {

/// This helper function will try to find a predecessor node of name "Name" 
/// and then return a list with the children nodes accessed in order to
/// find the predecessor, or NULL if it failed to find it.
/// If the node was found, but has a responsible node (parent) that
/// responds for it, the responsible node is returned in Resp and
/// the list addresses this specific node.
std::list<unsigned>* findPredecessor(SDNode* DAG, const std::string &Name, 
				     SDNode** Resp) {
  bool HasChain = false;
  try {
    const LLVMNodeInfo *Info = LLVMNodeInfoMan::getReference()
      ->getInfo(*DAG->OpName);    
    if (Info->HasChain) {
      HasChain = true;
    }
  } catch (LLVMDAGInfo::NameNotFoundException&) {}          
    
  for (unsigned i = 0; i < DAG->NumOperands; ++i) {
    const unsigned index = HasChain? i+1 : i;
    std::list<unsigned>* L = NULL;        
    if (*DAG->ops[i]->OpName == Name) {
      L = new std::list<unsigned>();      
      L->push_back(index);
      return L;
    }
    L = findPredecessor(DAG->ops[i], Name, Resp);
    if (L != NULL) {
      try {
	  const LLVMNodeInfo *Info = LLVMNodeInfoMan::getReference()
	    ->getInfo(*DAG->ops[i]->OpName);
	  if (!Info->MatchChildren) {
	    *Resp = DAG->ops[i];
	    delete L;
	    L = new std::list<unsigned>();
	    L->push_back(index);
	    return L;
	  }
      } catch (LLVMDAGInfo::NameNotFoundException&) {}
      L->push_front(index);
      return L;
    }
  }
  return NULL;
}

}; // end of anonymous namespace

SDNode *PatternTranslator::parseLLVMDAGString(const std::string &S) {
  unsigned dummy = 0;
  return parseLLVMDAGString(S, &dummy);
}

/// Converts an LLVM DAG string (extracted from the rules file, when specifying
/// an LLVM pattern) to an internal memory representation. 
/// Recursive function.
SDNode *PatternTranslator::parseLLVMDAGString(const std::string &S,
					      unsigned *pos) { 
  unsigned i = *pos;
  for (; i != S.size(); ++i) if (isalnum(S[i]) || S[i] == '(' 
                                 || S[i] == ')') break;
  if (S[i] == ')')
    return NULL;
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
  list<SDNode*> children;
  while (SDNode* child = parseLLVMDAGString(S, &i)) {
    children.push_back(child);
  }
  // All children parsed, Now eat the closing parenthesis
  for (; i != S.size(); ++i) if (S[i] == ')') break;  
  // Mismatched parenthesis
  if (i == S.size()) {
    for (list<SDNode*>::const_iterator I = children.begin(),
	  E = children.end(); I != E; ++I) {
      delete *I;
    }
    throw new PTParseErrorException();
  }
  ++i; // Eat ')';
  *pos = i;
  N->SetOperands(children.size());
  unsigned cur = 0;
  for (list<SDNode*>::const_iterator I = children.begin(),
	E = children.end(); I != E; ++I) {
    N->ops[cur++] = *I;
  }
  return N;
}

namespace {

/// Helper function used to build a string containing the name
/// of a specific node. All this function needs to know is a
/// list of children indexes used to address the current node,
/// starting from the root node of the DAG.
string buildNodeName(const list<unsigned>& pathToNode) {
  stringstream SS;
  SS << "N";
  for (list<unsigned>::const_iterator I = pathToNode.begin(),
    E = pathToNode.end(); I != E; ++I) {
    SS << *I;
  }
  return SS.str();
}

/// Helper function used to declare leaf nodes when generating
/// emit code LLVM function.
void emitCodeDeclareLeaf(SDNode *N, SDNode *LLVMDAG, std::ostream &S, 
			 map<string, string> *TempToVirtual,
			 const list<unsigned>& pathToNode) {
  if (N->RefInstr != NULL)
   return;        
  const string NodeName = buildNodeName(pathToNode);
  if (N->LiteralIndex != 0) {
    S << "  SDValue " << NodeName << " = ";
    //Generated an index for a string table, included in
    //SelectionDAG class, and use it as target constant.
    S << "CurDAG->getTargetConstant("<< N->LiteralIndex 
      << "ULL, MVT::i32);" << endl;
    return;
  }
  // If not literal, then check if it matches some operand in LLVMDAG
  SDNode *Resp = NULL;  
  list<unsigned>* Res = findPredecessor(LLVMDAG, *N->OpName, &Resp);
  if (Res == NULL) {
    // If it does not match, then it is a temporary register.      
    // We need to check if this temporary has already been alloc'd
    if (TempToVirtual->find(*N->OpName) == TempToVirtual->end()) {      
      (*TempToVirtual)[*N->OpName] = NodeName;
      S << "  const TargetRegisterClass* TRC" << NodeName
      //TODO: Find the correct type (not just i32)
	<< " = findSuitableRegClass(MVT::i32);" << endl;
      S << "  assert (TRC" << NodeName << " != 0 &&"
	<< "\"Could not find a suitable regclass for virtual\");" << endl;      
      S << "  SDValue " << NodeName << " = ";
      S << "CurDAG->getRegister(CurDAG->getMachineFunction().getRegInfo()"
	<< ".createVirtualRegister(TRC" << NodeName << "), "
	<< "MVT::i32);" << endl; //TODO:change this	
    } else {
      S << "  SDValue " << NodeName << " = " 
	<< (*TempToVirtual)[*N->OpName] << ";" << endl;
    }
    return;
  }  
  // It matches a LLVMDAG operand...
  const LLVMNodeInfo *Info = NULL;
  try {
    Info = LLVMNodeInfoMan::getReference()
	  ->getInfo(*LLVMDAG->OpName);
    if (!Info->MatchChildren) {
      delete Res;
      Res = new std::list<unsigned>();
      Resp = NULL;
    }
  } catch (LLVMDAGInfo::NameNotFoundException&) {}
  S << "  SDValue " << NodeName << " = ";  
  if (Res->size() > 0) {
    Info = (Resp != NULL) ? LLVMNodeInfoMan::getReference()
			       ->getInfo(*Resp->OpName) : NULL;
  }  
  stringstream SS;
  SS << "N";
  for (list<unsigned>::const_iterator I = Res->begin(), E = Res->end();
       I != E; ++I) {
    SS << ".getOperand(" << *I << ")";
  }
  // Check if this kind of node has its own get-filter function
  if (Info != NULL && Info->GetNode != NULL) 
    S << Info->GetNode(SS.str());
  else {
    S << SS.str();
    S << ";" << endl;
  }
  return;
}

} // end of anonymous namespace

//              *********************************
//              ** Instruction Selection Phase **
//              *********************************

std::string PatternTranslator::genEmitSDNodeHeader(unsigned FuncID) {
  stringstream SS;
  SS << "SDNode* EmitFunc" << FuncID << "(SDValue& N);" << endl;
  return SS.str();
}

std::string PatternTranslator::genEmitSDNode(SearchResult *SR,
						const std::string& LLVMDAGStr,
						unsigned FuncID, 
					        LiteralMap* LMap) {
  std::stringstream SS;
  list<unsigned> pathToNode;
  SDNode *RootNode = generateInsnsDAG(SR, LMap);
  SDNode *LLVMDAG  = parseLLVMDAGString(LLVMDAGStr);
  SS << "SDNode* __arch__`'DAGToDAGISel::EmitFunc" << FuncID 
     << "(SDValue& N) {" << endl;
  genEmitSDNode(RootNode, LLVMDAG, pathToNode, SS);
  SS << "}" << endl << endl;
  delete RootNode;
  delete LLVMDAG;
  return SS.str();
}

/// This function translates a SearchResult record to C++ code to emit
/// the instructions indicated by SearchResults in the LLVM backend, helping
/// to build the Code Generator.

/// The num parameter is used to compose the name of the C++ function that
/// will be generated.

void PatternTranslator::genEmitSDNode(SDNode* N, 
					 SDNode* LLVMDAG,
					 const list<unsigned>& pathToNode,
					 std::ostream &S) {  
  map<string, string> TempToVirtual;
  const LLVMNodeInfo *Info = NULL;
  
  if (pathToNode.empty()) 
    Info = LLVMNodeInfoMan::getReference()->getInfo(*LLVMDAG->OpName);
     
  // Depth first
  assert (N->RefInstr != NULL && "Must be valid instruction");    
  if (N->NumOperands != 0) {
    for (unsigned i = 0; i < N->NumOperands; ++i) {
      if (N->ops[i]->RefInstr != NULL) {
	list<unsigned> childpath = pathToNode;	
	if (pathToNode.empty() && Info->HasChain) { // Correct indexes if chain
	  childpath.push_back(i+1);
	  genEmitSDNode(N->ops[i], LLVMDAG, childpath, S);
	} else {
	  childpath.push_back(i);
	  genEmitSDNode(N->ops[i], LLVMDAG, childpath, S);
	}
      }
    }            
  }
  
  // Declare our leaf nodes. Non-leaf nodes are already declared
  // due to recursion.
  for (unsigned i = 0; i < N->NumOperands; ++i) {
    list<unsigned> childpath = pathToNode;	
    if (pathToNode.empty() && Info->HasChain) { // Correct indexes if chain
      childpath.push_back(i+1);
      emitCodeDeclareLeaf(N->ops[i], LLVMDAG, S, &TempToVirtual, childpath);
    } else {
      childpath.push_back(i);
      emitCodeDeclareLeaf(N->ops[i], LLVMDAG, S, &TempToVirtual, childpath); 
    }
  }  
  
  const string NodeName = buildNodeName(pathToNode);
  const int OutSz = N->RefInstr->getOutSize();  
  const bool HasChain = (pathToNode.empty() && Info->HasChain) || OutSz <= 0;
  const bool HasInFlag = (pathToNode.empty() && Info->HasInFlag);
  const bool HasOutFlag = (pathToNode.empty() && Info->HasOutFlag);
  // Declare our operand vector
  if (N->NumOperands > 0) {
    S << "  SDValue Ops" << NodeName << "[] = {";        
    for (unsigned i = 0; i < N->NumOperands; ++i) {
      list<unsigned> childpath = pathToNode;
       // Correct indexes if has chain
      if (HasChain) {
	childpath.push_back(i+1);
      } else { 
	childpath.push_back(i);
      }
      S << buildNodeName(childpath);      
      if (i != N->NumOperands-1)
	S << ", ";
    }    
    if (HasChain) {
      S << ", N.getOperand(0)";
    }
    // HasFlag? If yes, we must receive it as the last operand 
    if (HasInFlag && HasChain) 
      S << ", N.getOperand(" << LLVMDAG->NumOperands+1 << ")"; 
    else if (HasInFlag) 
      S << ", N.getOperand(" << LLVMDAG->NumOperands << ")"; 
    S << "};" << endl;
  } else {
    if (HasChain || HasInFlag)
      S << "  SDValue Ops" << NodeName << "[] = {";
    // HasChain? If yes, we must receive it as the first operand
    if (HasChain)      
      S << "N.getOperand(0)";
    // HasFlag? If yes, we must receive it as the last operand 
    if (HasInFlag && HasChain) 
      S << ", N.getOperand(" << LLVMDAG->NumOperands+1 << ")";
    else if (HasInFlag && !HasChain)
      S << "N.getOperand(" << LLVMDAG->NumOperands << ")";
    if (HasChain || HasInFlag)
      S << "};" << endl;
  }
  
  // If not level 0, declare ourselves by requesting a regular node
  if (!pathToNode.empty()) {    
    S << "  SDValue " << NodeName 
      << " = SDValue(CurDAG->getTargetNode(";
  } else {
    S << "  return CurDAG->SelectNodeTo(N.getNode(), ";
  }
  
  //TODO: Replace hardwired Sparc16!
  S << "Sparc16::" << N->RefInstr->getLLVMName();  
  if (OutSz <= 0)
    S << ", MVT::Other";
  else {    
    S << ", MVT::i" << OutSz;
    if (HasChain)
      S << ", MVT::Other";
  }
  if (HasOutFlag)
    S << ", MVT::Flag";
  
  if (N->NumOperands > 0 || HasChain) {
    S << ", Ops" << NodeName << ", ";
    int NumOperands = N->NumOperands;
    if (HasChain)
      ++NumOperands;
    if (HasInFlag)
      ++NumOperands;            
    S << NumOperands;
  }
  
  if (!pathToNode.empty())
    S << "), 0);" << endl;
  else
    S << ");" << endl;    
  
  return;
}

// MATCHER

inline void AddressOperand(std::ostream &S, vector<int>& Parents,
			   vector<int>& ChildNo, int i) {  
  list<int> Q;
  int p = Parents[i], prev = i;
  while (prev != -1) {
    Q.push_back(prev);    
    prev = p;
    if (p != -1)
      p = Parents[p];
  }
  while (!Q.empty()) {
    const int index = Q.back();
    Q.pop_back();
    S << "->getOperand(" << ChildNo[index] << ").getNode()";
  }
}

/// Generates the C++ code for the LLVM DAGISel to decide if the current
/// node is the desired pattern, and then call the appropriate emit
/// code function.
void PatternTranslator::genSDNodeMatcher(const std::string &LLVMDAG, map<string, MatcherCode> &Map,
					const string &EmitFuncName) {  
  list<SDNode *> Queue;
  vector<int> Parents; 
  vector<int> ChildNo;
  SDNode * Root = parseLLVMDAGString(LLVMDAG);
  LLVMNodeInfoMan *InfoMan = LLVMNodeInfoMan::getReference();  
  string RootName = InfoMan->getInfo(*Root->OpName)->EnumName;
  stringstream S;
  
  if (InfoMan->getInfo(*Root->OpName)->MatchChildren) {    
  if (Root->NumOperands > 0) {
    S << "if (";
  }
  bool RootHasChain = InfoMan->getInfo(*Root->OpName)->HasChain;
  for (int i = 0; static_cast<unsigned>(i) < Root->NumOperands; ++i) {
    Queue.push_back(Root->ops[i]);
    Parents.push_back(-1);
    if (RootHasChain)
      ChildNo.push_back(i+1);
    else
      ChildNo.push_back(i);
  }
  int i = -1;
  while (Queue.size() > 0) {
    i++;
    SDNode *N = Queue.front();
    Queue.pop_front();
    
    // First condition must not have "&&" as prefix
    if (i != 0)
      S << " && ";
    // check if leaf
    if (N->NumOperands == 0) {
      S << "N";
      AddressOperand(S, Parents, ChildNo, i);
      S << "->getValueType(0).getSimpleVT() == MVT::" << N->TypeName;
      continue;
    }
    // not leaf
    const LLVMNodeInfo *Info = InfoMan->getInfo(*N->OpName);
    S << "N";
    AddressOperand(S, Parents, ChildNo, i);
    S << "->getOpcode() == " << Info->EnumName;
    if (Info->MatchChildren) {      
      for (unsigned j = 0; j < N->NumOperands; ++j) {      	
	Queue.push_back(N->ops[j]);
	Parents.push_back(i);
	if (Info->HasChain)
	  ChildNo.push_back(j+1);
	else
	  ChildNo.push_back(j);
      }
    }
  }
  if (Root->NumOperands > 0) {
    S << ")" << endl;
  }
  }
    
  S << "  return " << EmitFuncName << "(Op);" << endl;
  
  if (Map.find(RootName) == Map.end()) {
    stringstream Prolog, Epilog;
    MatcherCode MC;
    Prolog << "case " << InfoMan->getInfo(*Root->OpName)->EnumName << ":" << endl;
    Epilog << "  break;" << endl;
    MC.setCode(S.str());
    MC.setProlog(Prolog.str());
    MC.setEpilog(Epilog.str());
    Map[RootName] = MC;
    return;
  }
  Map[RootName].AppendCode(S.str());
  return;
}


//              *********************************
//              ** Instruction Scheduler Phase **
//              *********************************

/// This function translates a SearchResult record to C++

/// generateEmitCode is used for building patterns in Instruction Selection
/// phase, while generateMachineCode is used for building patterns in
/// Instruction Scheduling phase.

/// In LLVM terminology, the former builds SDNodes, while the latter
/// builds MIs

// This translation from our internal notation to LLVM's MachineInstructions
// should be much more natural, because it uses quadruples and not DAGs, thus
// corresponding better to our internal notation. For example, we don't need
// to convert our internal notation to a DAG as an intermediary step for 
// translation.

std::string PatternTranslator::genEmitMI(SearchResult *SR, const StringMap& Defs,
					 LiteralMap *LMap,
					 bool alreadySorted,
					 bool mayUseVirtualReg,
					 list<const Register*>* auxiliarRegs,
					 unsigned ident,
					 const StringMap* ExtraParams,
					 const string& MBB,
					 const string& Itr,
					 const string& get){
  std::stringstream SS, DeclarationsStream;
  set<string> DeclaredVirtuals;  
  assert( (mayUseVirtualReg || auxiliarRegs != NULL) &&
         "Must provide auxiliar regs list if not mayUseVirtualReg");
  list<const Register*>::const_iterator currentAuxReg;
  if (auxiliarRegs != NULL) {
    currentAuxReg = auxiliarRegs->begin();
  }
  OperandsDefsType::const_iterator OI = SR->OperandsDefs->begin();
  for (InstrList::const_iterator I = SR->Instructions->begin(),
	 E = SR->Instructions->end(); I != E; ++I) {
    CnstOperandsList* AllOps = I->first->getOperandsBySemantic();
    CnstOperandsList* Outs = I->first->getOuts();
    BindingsList* Bindings = I->second->OperandsBindings;
    assert (OI != SR->OperandsDefs->end() && "SearchResult inconsistency");
    NameListType* OpNames = *(OI++);
    if (!alreadySorted)
      sortOperandsDefs(OpNames, I->second);
    assert (Outs->size() <= 1 && "FIXME: Expecting only one definition");
    const Operand *DefOperand = (Outs->size() == 0)? NULL: *(Outs->begin());
    for (unsigned i = 0; i < ident; ++i)
      SS << " "; 
    SS << "BuildMI(" << MBB << ", " << Itr << ", " << get << "(";
    SS << "__arch__`'::" << I->first->getLLVMName();  
    SS << ")";
    stringstream SSOperands;
            
    if (Bindings != NULL) {
      assert(OpNames->size() + Bindings->size() == AllOps->size()
	     && "Inconsistency. Missing sufficient bindings?");
    } else {
      assert(OpNames->size() == AllOps->size() 
	     && "Inconsistency. Missing sufficient bindings?");
    }
    unsigned i = 0;
    bool hasDef = false;
    NameListType::const_iterator NI = OpNames->begin();
    for (CnstOperandsList::const_iterator I2 = AllOps->begin(), 
	   E2 = AllOps->end(); I2 != E2; ++I2) {
      const Operand *Element = *I2;     
      // Do not include outs operands in input list
      if (reinterpret_cast<const void*>(Element) == 
	  reinterpret_cast<const void*>(DefOperand) &&
	  !hasDef) {
	// This is the defined operand
	assert (dynamic_cast<const RegisterOperand*>(Element) &&
	  "Currently only support register operand definition");
	SS << ", " << *NI;
	++NI;
	++i;
	hasDef = true;
	continue;
      }
      // DUMMY
      if (Element->getType() == 0) {		
	// See if bindings list has a definition for it
	if (Bindings != NULL) {
	  bool Found = false;
	  for (BindingsList::const_iterator I = Bindings->begin(),
		 E = Bindings->end(); I != E; ++I) {
	    if (HasOperandNumber(I->first)) {
	      if(ExtractOperandNumber(I->first) == i + 1) {
		unsigned index = LMap->addMember(I->second);
		SSOperands << ".addImm("<< index << ")";
		Found = true;
		break;
	      }
	    }
	  }
	  assert (Found && "Missing bindings for dummy operand");
	  ++i;
	  continue;
	}
	// No bindings for this dummy
	// TODO: ABORT? Wait for side effect compensation processing?
	SSOperands << ".addImm(0)";		
	++i;
	continue;
      }
      // NOT DUMMY
      assert(NI != OpNames->end() && 
	     "BUG: OperandsDefs lacks required definition");
      // Is this operand already defined?
            
      if (Defs.find(*NI) != Defs.end()) {	
	StringMap::const_iterator CI = Defs.find(*NI);
	SSOperands << ".add" << CI->second << "(" << *NI;
	StringMap::const_iterator El;
	// Check for extra parameters to be passed in order to create
	// this specific operand.
	if (ExtraParams != NULL && 
	   (El = ExtraParams->find(*NI)) != ExtraParams->end()) {
	  SSOperands << ", " << El->second;
	}
	SSOperands << ")";	
	++NI;
	++i;
	continue;
      }
      // This operand type is not defined, so it must be a scratch
      // register            
      // Test to see if this is a RegisterOperand
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(Element);
      if (RO != NULL && mayUseVirtualReg) {
	if (DeclaredVirtuals.find(*NI) == DeclaredVirtuals.end()) {      
	  DeclaredVirtuals.insert(*NI);
	  DeclarationsStream << "  const TargetRegisterClass* TRC" << *NI
	  //TODO: Find the correct type (not just i32)
	    << " = findSuitableRegClass(MVT::i32, " << MBB << ");" << endl;
	  DeclarationsStream << "  assert (TRC" << *NI << " != 0 &&"
	    << "\"Could not find a suitable regclass for virtual\");" << endl;
	  DeclarationsStream << "  unsigned " << *NI << " = ";
	  DeclarationsStream << MBB << ".getParent()->getRegInfo()"
	    << ".createVirtualRegister(TRC" << *NI << "); " <<  endl;
	}
	SSOperands << ".addReg(" << *NI << ")";	
	++NI;
	++i;
	continue;
      } else if (RO != NULL) {
	// We may not use virtual registers, so we need to use a scratch
	// register.
	if (DeclaredVirtuals.find(*NI) == DeclaredVirtuals.end()) {      
	  assert (auxiliarRegs != NULL);
	  assert (currentAuxReg != auxiliarRegs->end() && 
	          "Not enough auxiliar registers in architecture.");
	  DeclaredVirtuals.insert(*NI);
	  DeclarationsStream << "  unsigned " << *NI << " = ";
	  DeclarationsStream << "__arch__`'::" 
	                     << (*currentAuxReg)->getName() << ";" << endl;
	  ++currentAuxReg;
	}
	// The suffix false, false, true sets this to isKill = true, because
	// we need to kill the aux reg since it is a physical reg and we
	// will no longer need this definition.
	SSOperands << ".addReg(" << *NI << ", false, false, true)";	
	++NI;
	++i;
	continue;
      }
      // Not a scratch register operand... 
      assert (0 && "Non-scratch operands must be defined in DefList");
    }    
    SS << ")" << SSOperands.str();
    SS << ";" << endl;
    // Housekeeping
    delete AllOps;
    delete Outs;
  }
  
  // Organizing code
  DeclarationsStream << SS.str();
  return DeclarationsStream.str();
}

namespace {
inline void generateIdent(std::ostream& O, unsigned ident) {
  for (unsigned i = 0; i < ident; ++i)
    O << " ";
}

// Used in genIdentifyMI, see more details at local var Levels.
struct Level {
  stringstream *decl, *cond, *action;
  string instr;  
};
}

/// This member function generates LLVM backend C++ code to identify a known
/// pattern expressed as MachineInstructions (MI), that is, in the final phase
/// of code generation. If the identification is positive and DefsMap is
/// populated, it also generates code to extract the pattern operands, 
/// storing them in the variable names defines in the DefsMap.

// Expects the MachineInstr containing the root instruction of the pattern
// to be in scope with the name "pMI".
// Expects the function "findNearestDef" to be in scope, returning a
// MachineInstr that defines a requested virtual register.
string PatternTranslator::genIdentifyMI(SearchResult *SR,
					const StringMap& Defs,
					LiteralMap *LMap,
					const string &Action,
					const std::string &MI,
					unsigned ident) {  
  // This list stores, for each level of comparison, a pair containing code
  // that positively identifies a portion of the pattern and its respective
  // necessary declarations in order to execute the comparison (if statement).
  // As the pattern is being recognized, the code advances in the chain
  // of if conditions. Each if condition is a level. If the pattern fails
  // any level, it is not recognized.
  list<Level> Levels;
  list<Level>::iterator CurLevel;  
  Levels.resize(1);
  CurLevel = Levels.begin();
  CurLevel->instr = string(MI);
  CurLevel->action = new stringstream();
  CurLevel->decl = new stringstream();
  CurLevel->cond = new stringstream();
  stringstream FinalAction;
  unsigned curIdent = ident;
    
  map<string, Level *> DeclaredVirtuals;  
    
  OperandsDefsType::reverse_iterator OI = SR->OperandsDefs->rbegin();
  for (InstrList::reverse_iterator I = SR->Instructions->rbegin(),
	 E = SR->Instructions->rend(); I != E; ++I) {    
    CnstOperandsList* AllOps = I->first->getOperandsBySemantic();
    CnstOperandsList* Outs = I->first->getOuts();
    BindingsList* Bindings = I->second->OperandsBindings;
    assert (OI != SR->OperandsDefs->rend() && "SearchResult inconsistency");
    NameListType* OpNames = *(OI++);
    curIdent = curIdent + 2;
    // Already sorted, as this pattern was probably already emitted
    //sortOperandsDefs(OpNames, I->second);
    assert (Outs->size() <= 1 && "FIXME: Expecting only one definition");
    const Operand *DefOperand = (Outs->size() == 0)? NULL: *(Outs->begin());    
    if (Bindings != NULL) {
      assert(OpNames->size() + Bindings->size() == AllOps->size()
	     && "Inconsistency. Missing sufficient bindings?");
    } else {
      assert(OpNames->size() == AllOps->size() 
	     && "Inconsistency. Missing sufficient bindings?");
    }
    
    // If this instruction does not have a definition and is not the root,
    // there is no possible way we can identify this pattern because there
    // are instructions in the pattern that can't be reached via root!
    if (CurLevel != Levels.begin() && DefOperand == NULL) {
      assert(0 && "Multiple defs patterns not implemented!");
    }
    if (CurLevel == Levels.begin()) {
      *CurLevel->cond << CurLevel->instr << " != NULL && " << CurLevel->instr 
		    << "->getOpcode() == __arch__`'::" 
		    << I->first->getLLVMName() << " ";                
    }
    // We need to discover how this is instruction is declared. In order to
    // do this, we traverse the operand list, find the defined reg name
    // and infer the name of the variable used to declare this instruction.
    if (DefOperand != NULL) {      
      bool found = false;
      NameListType::const_iterator NI = OpNames->begin();
      for (CnstOperandsList::const_iterator I2 = AllOps->begin(), 
	   E2 = AllOps->end(); I2 != E2; ++I2) {
	const Operand *Element = *I2;     
	if (reinterpret_cast<const void*>(Element) == 
	    reinterpret_cast<const void*>(DefOperand)) {
	  // This is the defined operand
	  assert (dynamic_cast<const RegisterOperand*>(Element) &&
	      "Currently only support register operand definition");
	  if (CurLevel == Levels.begin()) {	    
	    if (Defs.find(*NI) != Defs.end()) {	
	      StringMap::const_iterator CI = Defs.find(*NI);
	      assert (CI->second == "Reg" && "Defined operand must be reg");
	      *CurLevel->cond << " && " << CurLevel->instr 
			    << "->getOperand(0).isReg()";
	      generateIdent(FinalAction, curIdent);
	      FinalAction << *NI << "= " << CurLevel->instr 
			  << "->getOperand(0).getReg();" << endl;			   
	    }
	  } else {
	    CurLevel->instr = *NI;
	    CurLevel->instr.append("_MI");
	    // If this name was not already declared by a previous instruction,
	    // then something is very wrong, i.e., there are multiple defs in
	    // this pattern.
	    assert(DeclaredVirtuals.find(*NI) != DeclaredVirtuals.end() 
	           && "Multiple defs patterns not implemented!");
	  }	  
	  found = true;
	  break;
	}
      }      
      assert ((CurLevel == Levels.begin() || found) 
	      && "DefOperand not found!");
    }
    // Build the first condition of this level, identifying the instruction
    // opcode
    if (CurLevel != Levels.begin()) {
      *CurLevel->cond << CurLevel->instr << " != NULL && " << CurLevel->instr 
		    << "->getOpcode() == __arch__`'::" 
		    << I->first->getLLVMName() << " ";                
    }
    // Now we simply traverse the operands and build the conditional string to
    // positively identify this pattern
    unsigned i = 0;
    if (DefOperand != NULL)
      i = 1;
    NameListType::const_iterator NI = OpNames->begin();
    for (CnstOperandsList::const_iterator I2 = AllOps->begin(), 
	   E2 = AllOps->end(); I2 != E2; ++I2) {
      const Operand *Element = *I2;     
      if (reinterpret_cast<const void*>(Element) == 
	  reinterpret_cast<const void*>(DefOperand)) {
	// Already handled	
	++NI;
	continue;
      }
      // DUMMY
      if (Element->getType() == 0) {		
	// See if bindings list has a definition for it
	if (Bindings != NULL) {
	  bool Found = false;
	  for (BindingsList::const_iterator I = Bindings->begin(),
		 E = Bindings->end(); I != E; ++I) {
	    if (HasOperandNumber(I->first)) {
	      const unsigned i_temp = DefOperand? i : i+1;
	      if(ExtractOperandNumber(I->first) == i_temp) {
		unsigned index = LMap->getMember(I->second);
		*CurLevel->cond << " && " << CurLevel->instr 
		               << "->getOperand(" << i << ").isImm() && " 
		               << CurLevel->instr << "->getOperand(" << i 
		               << ").getImm() == " << index;
		Found = true;
		break;
	      }
	    }
	  }
	  assert (Found && "Missing bindings for dummy operand");
	  ++i;
	  continue;
	}
	// No bindings for this dummy
	// TODO: ABORT? Wait for side effect compensation processing?
	assert(0 && "Side effect compensation not detectable");
	++i;
	continue;
      }
      // NOT DUMMY
      assert(NI != OpNames->end() && 
	     "BUG: OperandsDefs lacks required definition");
      // Is this operand already defined?
            
      if (Defs.find(*NI) != Defs.end()) {
	StringMap::const_iterator CI = Defs.find(*NI);
	*CurLevel->cond << " && " << CurLevel->instr << "->getOperand(" << i
	               << ").is" << CI->second << "()";
	generateIdent(FinalAction, curIdent);
	FinalAction << *NI << " = " << CurLevel->instr << "->getOperand("
	                 << i << ").get" << CI->second << "();" << endl;
	++NI;
	++i;
	continue;
      }
      // This operand type is not defined, so it must be a scratch
      // register            
      // Test to see if this is a RegisterOperand
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(Element);
      if (RO != NULL) {
	if (DeclaredVirtuals.find(*NI) == DeclaredVirtuals.end()) {      
	  // This is important, since here we declare a MachineInstr* that
	  // may be used in the future to continue identification of the
	  // pattern. This name is later recovered in the first loop.
	  DeclaredVirtuals[*NI] = &*CurLevel;	  
	  generateIdent(*CurLevel->decl, curIdent-2);
	  generateIdent(*CurLevel->action, curIdent);
	  *CurLevel->decl << "int " << *NI << " = (" << i << " < " 
			 << CurLevel->instr 
	                 << "->getNumOperands() && " 
	                 << CurLevel->instr 	  
	                 << "->getOperand(" << i << ").isReg()) ? (int)" 
	                 << CurLevel->instr << "->getOperand(" << i 
	                 << ").getReg() : -1;" <<  endl;	  
	  *CurLevel->action << "const MachineInstr* " << *NI 
	                   << "_MI = findNearestDef(" 
	                    << CurLevel->instr << "," << *NI
	                   << ");" << endl;
	}
	*CurLevel->cond << " && " << CurLevel->instr << "->getOperand(" << i
	               << ").isReg() && " << CurLevel->instr << "->getOperand("
	               << i << ").getReg() == (unsigned) " << *NI;
	++NI;
	++i;
	continue;
      }
      // Not a scratch register operand... 
      assert (0 && "Non-scratch operands must be defined in DefList");
    }        
    // Housekeeping
    delete AllOps;
    delete Outs;
    Levels.resize(Levels.size()+1);
    CurLevel++;
    CurLevel->action = new stringstream();
    CurLevel->decl = new stringstream();
    CurLevel->cond = new stringstream();
  }
  
  stringstream SS;
  // Organizing code
  unsigned NumParen = 0;
  curIdent = ident - 2;
  for (list<Level>::iterator I = Levels.begin(), E = Levels.end(); I!= E; ++I){
    curIdent = curIdent + 2;
    // Check this because the last Level is empty and must be avoided
    if (I->cond->str().size() > 0) {
      SS << I->decl->str();
      generateIdent(SS, curIdent);
      SS << "if (" << I->cond->str() << ") {" << endl;      
      SS << I->action->str();
      ++NumParen;    
    }
    delete I->action;
    delete I->decl;
    delete I->cond;
  }
  SS << FinalAction.str();
  generateIdent(SS, curIdent);
  SS << Action << endl;
  
  for (unsigned I = 0; I < NumParen; ++I) {
    curIdent = curIdent - 2;
    generateIdent(SS, curIdent);
    SS << "}" << endl;
  }  
  return SS.str();
}
