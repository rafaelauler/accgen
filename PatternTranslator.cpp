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
  Chain = NULL;
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
  if (Chain)
    delete Chain;
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
	// FIXME: Should be removed by SR->FilterAssignedNames();
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
		if (E == 0)
			E = 1;
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

namespace {
void ReportOperandsMismatch(std::ostream &S, CnstOperandsList *AllOps,
			    BindingsList *Bindings, NameListType *OpNames,
			    InstrList::const_iterator I) {
  S << "Error using instruction \"" << (*I).first->getLLVMName();
  if (Bindings == NULL) {
    S << "\". Described semantics uses " << AllOps->size()
      << " operands, but search algorithm expected "
      << OpNames->size() << ".\n";
  } else {
    S << "\". Described semantics uses " << (AllOps->size() + Bindings->size())
      << " operands, but search algorithm expected "
      << OpNames->size() << ".\n";
  }  
      
  int u = 0;
  S << "Printing list of extracted semantic operands:\n";
  for (CnstOperandsList::const_iterator I2 = AllOps->begin(), 
    E2 = AllOps->end(); I2 != E2; ++I2) {
    S << "Node " << u << ": ";
    (*I2)->print(S);
    S << "\n";
    ++u;
  }
  if (Bindings != NULL) {
    u = 0;   
    for (BindingsList::const_iterator I2 = Bindings->begin(),
	 E = Bindings->end(); I2 != E; ++I2) {
      S << "Binding " << u << ": ";
      S << I2->first;
      S << "\n";
      ++u;      
    }
  }
}
}

// Translate a SearchResult with a pattern implementation to DAG-like
// syntax used when defining a "Pattern" object in XXXInstrInfo.td
// This DAG is connected by uses relations and nodes contain only "ins"
// operands.

// Q: What to do when the root of the dag assigns to a register
// bound by VirtualToRealmap? How to handle Defs by Real registers?
// Ans: GenerateInstrDefs from TemplateManager will generate a "def"
// for the register in XXXInstrInfo.td, and this is enough.
SDNode* PatternTranslator::generateInsnsDAG(SearchResult *SR, 
  LiteralMap* LMap) {
  DefList Defs;
  SDNode *LastProcessedNode = NULL, *NoDefNode = NULL;  
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
    // to put it as the root of the DAG, or, if there is already a root,
    // put it as a chain operand of the last processed instruction just to
    // ensure order.
    if (Outs->size() == 0) {
      if (NoDefNode != NULL) {
	N->Chain = NoDefNode;
	NoDefNode = N;
      } else {
	NoDefNode = N;
      }
    }
    unsigned i = 0;
    if (Bindings != NULL) {
      if (OpNames->size() + Bindings->size() != AllOps->size()) {
	std::stringstream sstmp;
	ReportOperandsMismatch(sstmp, AllOps, Bindings, OpNames, I);
	AbortOn(sstmp.str());
      }       
    } else {
      if (OpNames->size() != AllOps->size()) {
	std::stringstream sstmp;
	ReportOperandsMismatch(sstmp, AllOps, Bindings, OpNames, I);
	AbortOn(sstmp.str());
      }      
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
	  bool Found = false;
	  for (BindingsList::const_iterator I = Bindings->begin(),
		 E = Bindings->end(); I != E; ++I) {
	    if (HasOperandNumber(I->first)) {
	      unsigned OpNum = HasDef? i + 2 : i + 1;
	      if(ExtractOperandNumber(I->first) == OpNum) {
		N->ops[i]->LiteralIndex = LMap->addMember(I->second);
		// Here we leave N->TypeName empty, because a 
		// dummy operand does not have type.
		Found = true;
		++i;
		break;
	      }
	    }
	  }
	  assert (Found && "Expected binding not found.");
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
                                 || S[i] == ')' || S[i] == '_') break;
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
  for (; i != S.size(); ++i) if (!(isalnum(S[i]) || S[i] == '_')) break;  
  
  // Premature end of string
  if (i == S.size())
    throw new PTParseErrorException();  
  SDNode* N = new SDNode();
  // Determine if we have type specification (':')
  if (S[i] != ':') {    
    N->OpName = new string(S.substr(begin, i-begin));
    // If this is a leaf, and does not have ":", then it is a constant that
    // needs to be checked by the matcher!
    // Inform this by using a nonzero value in LiteralIndex field.
    if (!hasChildren)
      N->LiteralIndex = 1;
  } else {
    N->TypeName = S.substr(begin, i-begin);    
    ++i; // Eat ':'
    if (S[i] != '$') {
      delete N;
      throw new PTParseErrorException();
    }
    ++i; // Eat '$'
    const unsigned namebegin = i;
    for (; i != S.size(); ++i) if (!(isalnum(S[i]) || S[i] == '_')) break;  
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

/// Reads all transformations available and discovers which ones applies to
/// the given operand. May be more than one, so the result is a list of
/// chained operations.
inline list<const OperandTransformation*>*
findOperandTransformation(const OpTransLists* OTL, const string& OpName) {
  string name(OpName);
  string baseLHS = name.substr(0,name.find('_'));
  string::size_type idx = 0;
  list<const OperandTransformation*>* Result = NULL;
  while ((idx = name.find('_', idx+1)) != string::npos) {    
    if (name.size() <= idx+3)
      break;
    string rhs = name.substr(idx+1, name.find('_', idx+1)-idx-1);
    string lhs = name.substr(0, idx);  
    for (OpTransLists::const_iterator OTLI = OTL->begin(),
      OTLE = OTL->end(); OTLI != OTLE; ++OTLI) {    
      for (OperandTransformationList::const_iterator I = OTLI->begin(),
	  E = OTLI->end(); I != E;
	  ++I) {
	if (I->RHSOperand == rhs && I->LHSOperand == lhs) {
	  if (Result == NULL)
	    Result = new list<const OperandTransformation*>();
	  Result->push_back(&*I);
	}    
      }
    }// end outer for
  }// end while
  return Result;
}

/// Helper function used to declare leaf nodes when generating
/// emit code LLVM function.
void emitCodeDeclareLeaf(SDNode *N, SDNode *LLVMDAG, std::ostream &S, 
			 map<string, string> *TempToVirtual,
			 const list<unsigned>& pathToNode,
			 const OpTransLists* OTL) {
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
  list<const OperandTransformation*>* L = NULL;
  list<unsigned>* Res = findPredecessor(LLVMDAG, *N->OpName, &Resp);
  if (Res == NULL) {
    // If it does not match, check to see if it is a constant node
    int ConstVal = 0;
    if (ExtractConstValue(*N->OpName, &ConstVal)) {
      S << "  SDValue " << NodeName << " = " << "CurDAG->getTargetConstant("
        << ConstVal << "ULL, MVT::i32);" << endl;
      return;
    }
    // Else, check if this operand is bound to another via transformation rules
    L = findOperandTransformation(OTL, *N->OpName);
    if (L != NULL) {
      assert (L->size() > 0);
      Res = findPredecessor(LLVMDAG, (*L->begin())->LHSOperand, &Resp);
    }
  }
  if (Res == NULL) {
    // Not found as predecessor. It must be a temporary register.      
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
    S << Info->GetNode(SS.str(), L);
  else { 
    S << SS.str();
    S << ";" << endl;
  }       
  if (L != NULL)
    delete L;
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
  genEmitSDNode(RootNode, LLVMDAG, pathToNode, SR->OpTrans, SS);
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
				         const OpTransLists* OTL,
					 std::ostream &S) {  
  map<string, string> TempToVirtual;
  const LLVMNodeInfo *Info = NULL;
  bool ChainTail = true; // Turns false if we need to chain a node
  
  if (pathToNode.empty()) 
    Info = LLVMNodeInfoMan::getReference()->getInfo(*LLVMDAG->OpName);
     
  // Depth first
  assert (N->RefInstr != NULL && "Must be valid instruction");    
  if (N->NumOperands != 0) {
    for (unsigned i = 0; i < N->NumOperands; ++i) {
      if (N->ops[i]->RefInstr != NULL) {
	list<unsigned> childpath = pathToNode;	
	childpath.push_back(i);
	genEmitSDNode(N->ops[i], LLVMDAG, childpath, OTL, S);	
      }
    }
    // Check for chain
    if (N->Chain) {
      assert (N->Chain->RefInstr != NULL && "Chain must be an instruction");
      ChainTail = false;
      list<unsigned> childpath = pathToNode;      
      childpath.push_back(N->NumOperands);
      genEmitSDNode(N->Chain, LLVMDAG, childpath, OTL, S);
    }    
  }
  
  // Declare our leaf nodes. Non-leaf nodes are already declared
  // due to recursion.
  for (unsigned i = 0; i < N->NumOperands; ++i) {
    list<unsigned> childpath = pathToNode;	    
    childpath.push_back(i);
    emitCodeDeclareLeaf(N->ops[i], LLVMDAG, S, &TempToVirtual, childpath, OTL);     
  }  
  
  const string NodeName = buildNodeName(pathToNode);
  const int OutSz = N->RefInstr->getOutSize();  
  const bool HasChain = (pathToNode.empty() && Info->HasChain) || OutSz <= 0;
  const bool HasInFlag = (pathToNode.empty() && Info->HasInFlag);
  const bool HasOutFlag = (pathToNode.empty() && Info->HasOutFlag);
  
  assert ((ChainTail || HasChain) && "If this node chains another node, it"
           " must have chain operand!");
  // Declare our operand vector
  if (N->NumOperands > 0) {
    S << "  SDValue Ops" << NodeName << "[] = {";        
    for (unsigned i = 0; i < N->NumOperands; ++i) {
      list<unsigned> childpath = pathToNode;       
      childpath.push_back(i);      
      S << buildNodeName(childpath);      
      if (i != N->NumOperands-1)
	S << ", ";
    }    
    if (HasChain && ChainTail) {
      S << ", N.getOperand(0)";
    } else if (HasChain && !ChainTail) {
      list<unsigned> childpath = pathToNode;
      S << ", ";
      childpath.push_back(N->NumOperands);
      S << buildNodeName(childpath);
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
    if (HasChain && ChainTail)      
      S << "N.getOperand(0)";
    else if (HasChain && !ChainTail) {
      list<unsigned> childpath = pathToNode;
      childpath.push_back(N->NumOperands);
      S << buildNodeName(childpath);
    }
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
  
  S << "`'__arch__`'::" << N->RefInstr->getLLVMName();  
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
  if (Root->NumOperands > 0 || 
      InfoMan->getInfo(*Root->OpName)->MatchNode) {
    S << "if (";
  }
  if (InfoMan->getInfo(*Root->OpName)->MatchNode) {
    S << InfoMan->getInfo(*Root->OpName)->MatchNode("N", ""); 
    if (Root->NumOperands > 0)
      S  << " && " << endl;
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
      if (N->LiteralIndex == 0) {
	S << "->getValueType(0).getSimpleVT() == MVT::" << N->TypeName;
      } else {
	//In this case, this node is a constant and needs to be correctly
	//mached.
	assert(0 && "Constant node cannot be matched. Use "
	       "MatchChildren=false in parent node.");
      }
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
    } else {
      // Check if this is a constant that needs to be matched
      if (N->NumOperands == 1 && N->ops[0]->LiteralIndex != 0) {
	assert (Info->MatchNode && "Matching function must exist for const"
	        " node");
	stringstream S2;
	S2 << "N";
	AddressOperand(S2, Parents, ChildNo, i);	
	S << " && " << Info->MatchNode(S2.str(), *N->ops[0]->OpName);
      } 
      // Check to see if it has a special matching function
      else if (Info->MatchNode) {
	stringstream S2;
	S2 << "N";
	AddressOperand(S2, Parents, ChildNo, i);	
	S << " && " << Info->MatchNode(S2.str(), "");
      }
      
    }
  }
  if (Root->NumOperands > 0) {
    S << ")" << endl;
  }
  } else if (InfoMan->getInfo(*Root->OpName)->MatchNode) {
    S << "if (" << InfoMan->getInfo(*Root->OpName)->MatchNode("N", "") 
      << ")" << endl;
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

namespace {
    
inline bool MIHandleOpAlreadyDefined(const StringMap& Defs,
				     const string& OpName,
				     const StringMap* ExtraParams,
				     stringstream &SSOperands) {
  if (Defs.find(OpName) != Defs.end()) {	
    StringMap::const_iterator CI = Defs.find(OpName);
    SSOperands << ".add" << CI->second << "(" << OpName;
    StringMap::const_iterator El;
    // Check for extra parameters to be passed in order to create
    // this specific operand.
    if (ExtraParams != NULL && 
      (El = ExtraParams->find(OpName)) != ExtraParams->end()) {
      SSOperands << ", " << El->second;
    }
    SSOperands << ")";		
    return true;
  }
  return false;
}

inline bool MIHandleScratchReg(const Operand* Element, bool mayUseVirtualReg,
			       const string& OpName,
			       const string& MBB,
			       set<string>& DeclaredVirtuals,
			       stringstream& DeclarationsStream,
			       stringstream& SSOperands,
			       bool isDef,
			       RegAlloc* RA) {
  const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(Element);
  if (!isDef || RA->IsLive(OpName)) {
    if (RO != NULL && mayUseVirtualReg) {
      if (DeclaredVirtuals.find(OpName) == DeclaredVirtuals.end()) {      
	DeclaredVirtuals.insert(OpName);
	DeclarationsStream << "  const TargetRegisterClass* TRC" << OpName
	//TODO: Find the correct type (not just i32)
	  << " = findSuitableRegClass(MVT::i32, " << MBB << ");" << endl;
	DeclarationsStream << "  assert (TRC" << OpName << " != 0 &&"
	  << "\"Could not find a suitable regclass for virtual\");" << endl;
	DeclarationsStream << "  unsigned " << OpName << " = ";
	DeclarationsStream << MBB << ".getParent()->getRegInfo()"
	  << ".createVirtualRegister(TRC" << OpName << "); " <<  endl;
      }
      if (isDef)
	SSOperands << ", " << OpName;
      else
	SSOperands << ".addReg(" << OpName << ")";	
      return true;
    } else if (RO != NULL) {
      // We may not use virtual registers, so we need to use a scratch
      // register.
      if (DeclaredVirtuals.find(OpName) == DeclaredVirtuals.end()) {      
	std::string PhysRegName = RA->getPhysRegister(OpName);
	DeclaredVirtuals.insert(OpName);
	DeclarationsStream << "  unsigned " << OpName << " = ";
	DeclarationsStream << "__arch__`'::" 
			  << PhysRegName << ";" << endl;
      }
      // The suffix false, false, true sets this to isKill = true, because
      // we need to kill the aux reg since it is a physical reg and we
      // will no longer need this definition.
      if (isDef)
	SSOperands << ", " << OpName;
      else
	SSOperands << ".addReg(" << OpName << ", false, false, true)";	
      return true;
    }
  } else if (isDef) {
    SSOperands << ", " << OpName;
    return true;
  }
  return false;
}

inline bool MIHandleTransformation(const OpTransLists* OTL, 
			           const string& OpName,
				   const StringMap& Defs,
				   stringstream& SSOperands) {
  list<const OperandTransformation*>* L = findOperandTransformation(OTL, OpName);
  if (L == NULL)
    return false;
  assert (L->size() > 0);  
  list<const OperandTransformation*>::const_iterator I, E;
  I = L->begin();
  ++I;
  E = L->end();
  const string& name = (*L->begin())->LHSOperand;
  string Exp = (*L->begin())->TransformExpression;
  Exp.insert((string::size_type)0,1,'(');
  Exp.append(1,')');
  for (; I != E; ++I) {
    Exp = (*I)->PatchTransformExpression(Exp);
    Exp.insert((string::size_type)0,1,'(');
    Exp.append(1,')');
  }
  if (Defs.find(name) != Defs.end()) {	    
    StringMap::const_iterator CI = Defs.find(name);
    //while (string::size_type i = trans.find(rhs))
    //  trans.erase(i, i+rhs.size());    
    SSOperands << ".add" << CI->second << "(" <<  Exp;        
    SSOperands << ")";	
    delete L;
    return true;
  }
  delete L;
  return false;
}
}

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
//TODO: HANDLE CHAIN
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
  RegAlloc *RA = NULL;
  assert(auxiliarRegs != NULL && "Must provide auxiliar regs list");  
  RA = RegAlloc::Build(auxiliarRegs, SR->OperandsDefs->begin(),
			 SR->OperandsDefs->end());  
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
    if (Itr == "")
      SS << "BuildMI(" << MBB << ", "; 
    else
      SS << "BuildMI(" << MBB << ", " << Itr << ", ";
    SS << get << "(`'__arch__`'::" << I->first->getLLVMName();  
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
	bool emit = MIHandleScratchReg(Element,  mayUseVirtualReg, *NI, MBB,
		DeclaredVirtuals, DeclarationsStream, SS, true, RA);
	assert (emit && "Currently only support register operand definition");
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
	  for (BindingsList::const_iterator I2 = Bindings->begin(),
		 E2 = Bindings->end(); I2 != E2; ++I2) {
	    if (HasOperandNumber(I2->first)) {
	      unsigned OpNum = i + 1;
	      if(ExtractOperandNumber(I2->first) == OpNum) {
		unsigned index = LMap->addMember(I2->second);
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
      if (MIHandleOpAlreadyDefined(Defs, *NI, ExtraParams, SSOperands)) {
	++NI;
	++i;
	continue;
      }
      // Check if this operand must be transformed based on 
      // another input operand.
      if (MIHandleTransformation(SR->OpTrans, *NI, Defs, SSOperands)) {
	++NI;
	++i;
	continue;
      }
      
      // This operand type is not defined, so it must be a scratch
      // register            
      // Test to see if this is a RegisterOperand
      if (MIHandleScratchReg(Element,  mayUseVirtualReg, *NI, MBB,
	DeclaredVirtuals, DeclarationsStream, SSOperands, false, RA)) {
	++NI;
	++i;
	continue;
      }
      // Test to see if this is an immediate operand bound to a const      
      int ConstVal = 0;
      if (dynamic_cast<const ImmediateOperand*>(Element) &&
	  ExtractConstValue(*NI, &ConstVal)) {
	SSOperands << ".addImm("<< ConstVal << ")";
	++NI;
	++i;
	continue;
      }
      
      // Not a scratch register operand... 
      assert (0 && "Non-scratch operands must be defined in DefList");
    }    
    SS << ")" << SSOperands.str();
    SS << ";" << endl;
    RA->NextInstruction();
    // Housekeeping
    delete AllOps;
    delete Outs;
  }
  
  // Organizing code
  DeclarationsStream << SS.str();
  return DeclarationsStream.str();
}

namespace {
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
/// The argument ErasePatt can be used to control whether the identified
/// instructions should be erased, once the pattern is confirmed.

// Expects the MachineInstr containing the root instruction of the pattern
// to be in scope with the name "pMI".
// Expects the function "findNearestDefBefore" to be in scope, returning a
// MachineInstr that defines a requested virtual register.

//TODO: HANDLE CHAIN
//TODO: Handle OperandTransformation
string PatternTranslator::genIdentifyMI(SearchResult *SR,
					const StringMap& Defs,
					LiteralMap *LMap,
					const string &Action,
					const std::string &MI,
					unsigned ident,
					bool ErasePatt) {  
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
  stringstream RemoveAction;
  unsigned curIdent = ident;
  // If we should delete the pattern once identified, schedule erase code to
  // run for the root as a final action;
  if (ErasePatt) {
    generateIdent(RemoveAction, curIdent);
    RemoveAction << CurLevel->instr << "->eraseFromParent();" << endl;
  }
    
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
    // We need to discover how this instruction is declared. In order to
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
	    // If this pattern should be erased once found, emit erase calls
	    // as final action
	    // NOTE: Do not erase intermediate instructions, as this may be
	    // used by other instructions, at this stage
	    if (ErasePatt) {
	      generateIdent(RemoveAction, curIdent);
	      RemoveAction << "if (!isLive(" << CurLevel->instr << ")) " 
	        << CurLevel->instr << "->eraseFromParent();" << endl;
	    }
	  }	  
	  found = true;
	  break;
	}
	if (Element->getType() != 0)
	  ++NI;
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
	  *CurLevel->action << "MachineInstr* " << *NI 
	                   << "_MI = findNearestDefBefore(" 
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
  if (ErasePatt)
    SS << RemoveAction.str();
  
  for (unsigned I = 0; I < NumParen; ++I) {
    curIdent = curIdent - 2;
    generateIdent(SS, curIdent);
    SS << "}" << endl;
  }  
  return SS.str();
}

inline bool ReachedRoot(InstrList* L, InstrList::iterator I) {
  ++I;
  if (I == L->end())
    return true;
  return false;
}

struct NoDefException{};

inline const std::string& ExtractDefOperandName(NameListType* OpNames, 
					 CnstOperandsList* AllOps,
					 const Operand* DefOperand) {
  NameListType::const_iterator NI = OpNames->begin();
  for (CnstOperandsList::const_iterator I2 = AllOps->begin(), 
	E2 = AllOps->end(); I2 != E2; ++I2) {
    const Operand *Element = *I2;     
    if (reinterpret_cast<const void*>(Element) == 
	reinterpret_cast<const void*>(DefOperand)) {
	return *NI;
      }
    if (Element->getType() != 0)
      ++NI;
  }
  throw NoDefException();
}

/// This member function emits C++ code to find the root of a given pattern
/// in MachineInstrs format, given a pattern operand. The code starts at the
/// pattern operand (MI that has this operand) and ends with the root MI, if
/// possible.

// Expects the function "findNearestUseAfter" to be in scope, returning a
// MachineInstr that defines a requested virtual register.
// TODO: handle chain
string PatternTranslator::genFindMIPatRoot(SearchResult *SR, 
					   const string& InitialMI,
					   const string& OperandName,					   
					   unsigned ident)
{
  bool FoundStart = false, FoundRoot = false;
  stringstream Code;
  unsigned count = 1;
  string curDefName;
  generateIdent(Code,ident);
  Code << "MachineInstr* MI1 = " << InitialMI << ";" << endl;
  generateIdent(Code,ident);
  Code << "MachineInstr* MIRoot = NULL;" << endl;
  OperandsDefsType::iterator OI = SR->OperandsDefs->begin();
  for (InstrList::iterator I = SR->Instructions->begin(),
	 E = SR->Instructions->end(); I != E; ++I) {    
    NameListType* OpNames = *(OI++);
    CnstOperandsList* AllOps = I->first->getOperandsBySemantic();
    CnstOperandsList* Outs = I->first->getOuts();
    assert (Outs->size() <= 1 && "FIXME: Expecting only one definition");    
    const Operand *DefOperand = (Outs->size() == 0)? NULL: *(Outs->begin());
    if (DefOperand == NULL && !ReachedRoot(SR->Instructions, I)) {
      // This insn does not have a definition and is not the root, so
      // there is no possible way we can identify this pattern because there
      // are instructions in the pattern that can't be reached via root!    
      assert(0 && "Multiple defs patterns not implemented!");
    }
    if (!FoundStart) {      
     for (NameListType::const_iterator NI = OpNames->begin(), 
	   NE = OpNames->end(); NI != NE; ++NI) {
       if (*NI == OperandName) {
	 FoundStart = true;
	 break;
       }
     }
     if (!FoundStart)
       continue;
     else {
       if (ReachedRoot(SR->Instructions, I)) {
	  generateIdent(Code, ident);
	  Code << "MIRoot = MI" << count << ";" << endl;	
	  FoundRoot = true;
	} else {
	  curDefName = ExtractDefOperandName(OpNames, AllOps, DefOperand);
	}
	count++;
	continue;
     }
    }    
    bool FoundDef = false;
    for (NameListType::const_iterator NI = OpNames->begin(), 
	 NE = OpNames->end(); NI != NE; ++NI) {
      if (*NI == curDefName) {
	FoundDef = true;
	break;
      }
    }
    if (!FoundDef)
      continue;
    
    generateIdent(Code, ident);
    Code << "if (MI" << count-1 << " != NULL) {" << endl;
    ident += 2;
    generateIdent(Code, ident);
    Code << "MachineInstr* MI" << count << " = ";
    Code << "findNearestUseAfter(MI" << count-1 << ", MI" << count-1
         << "->getOperand(0).getReg());" << endl;    
    if (ReachedRoot(SR->Instructions, I)) {
      generateIdent(Code, ident);
      Code << "MIRoot = MI" << count << ";" << endl;
      FoundRoot = true;
    } else {
      curDefName = ExtractDefOperandName(OpNames, AllOps, DefOperand);
    }
    count++;
  }
  assert (FoundStart && "Invalid OperandName for this pattern");
  assert (FoundRoot && "Operand doesn't reach root!");
  for (unsigned i = count; i > 2; --i) {
    ident -= 2;
    generateIdent(Code, ident);
    Code << "}" << endl;
  }
  return Code.str();
}
