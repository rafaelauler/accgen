//===- PatternTranslator.h - --------------------------------*- C++ -*-----===//
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

#ifndef PATTERNTRANSLATOR_H
#define PATTERNTRANSLATOR_H

//===-- Includes ---===//
#include "InsnSelector/Search.h"
#include "InsnSelector/Semantic.h"
#include "Instruction.h"
#include <map>

//===-- Class prototypes ---===//
namespace backendgen {
  // This is a simple class representing a selection DAG node. It is not as
  // complex as LLVM's SDNode because its purpose is to represent a simple
  // DAG as an intermediate step to translate from SearchResult to LLVM's
  // DAG notation.
  class SDNode {    
  public:
    const Instruction *RefInstr; // If this node refers to a machine instruction
    std::string *OpName;         // If this node refers to a leaf operand
    std::string TypeName;        // If this node refers to a leaf operand which
                                 // is a register operand, we need the
                                 // register class to use in TemplateManager::
                                 // PostprocessLLVMDAGString. Else, this
                                 // same member function will define this prop.
    // In case this node represents a leaf operand and is a literal value 
    // (i.e. in a "dummy" operand, defined by "let" clauses). In this case,
    // we need to write it using the LLVM Assembly Writter.
    // The index identifies the literal string in a map. If it is zero, then
    // this node is not literal.
    // Only used when translated from SearchResult. When translating from a
    // LLVMDAGString, used to flag a constant value that needs to be checked.
    unsigned LiteralIndex; 
    // If specific register, we need to print it differently, as LLVM will
    // already known this name and recognize as a register.
    bool IsSpecificReg;
                                 
    unsigned NumOperands;
    SDNode** ops;
    // This is a special operand. Its only purpose is to impose order
    // between two nodes that does not have any use-def connection normally
    // used by the DAG structure. 
    // Only used when translated from SearchRestult
    SDNode* Chain;  
    SDNode();
    ~SDNode();
    void SetOperands(unsigned num);
    void Print(std::ostream &S);
  };
  
  class MatcherCode {
    std::string Prolog;
    std::string Code;
    std::string Epilog;    
  public:
    std::string getProlog() { return Prolog; }
    std::string getCode() {return Code; }
    std::string getEpilog() { return Epilog; }
    void setProlog(const std::string& S) { Prolog = S; }
    void setCode(const std::string& S) { Code = S; }
    void setEpilog(const std::string& S) { Epilog = S; }
    void Print(std::ostream& S);
    void inline AppendCode(const std::string& S);
  };
  
  // This class stores a map of literal strings, that is, a string table
  // to store operands with a fixed name in the code gen. This LiteralMap
  // is later reproduced into AsmWritter backend class, so it can print
  // the correct strings since we use only int indexes to them into IL.
  class LiteralMap : public std::map<unsigned, std::string> {
    unsigned CurrentIndex;
  public:
    LiteralMap(): std::map<unsigned, std::string>(), CurrentIndex(1) {}
    unsigned addMember(const std::string& Name);
    unsigned getMember(const std::string& Name);
    void printAll(std::ostream& O);
  };
  
  struct PTParseErrorException {};  

  typedef std::map<std::string, std::string> StringMap;
  // This is the class responsible for translating from our SearchResult
  // internal notation to LLVM's DAG notation, so we can write InstrInfo and
  // ISelDAGToDAG backend files.
  //
  // Our notation is a list of instructions that implements a given expression
  // (pattern) in the target architecture. Also, we're given a list of
  // operands names for each instruction in this list. This may be seen as
  // quadruples intermediate representation. 
  //
  // LLVM's intemediate form is also quadruples. But when writing a backend,
  // tablegen limits us to express patterns in DAG syntax. We thus must have
  // methods to convert from quadruples do DAG.
  class PatternTranslator {
    OperandTableManager &OperandTable;
    void sortOperandsDefs(NameListType* OpNames, SemanticIterator SI);
    SDNode *generateInsnsDAG(SearchResult *SR, LiteralMap* LMap);
    SDNode *parseLLVMDAGString(const std::string &S);
    SDNode *parseLLVMDAGString(const std::string &S, unsigned *pos);
  public:
    // Constructor
    PatternTranslator(OperandTableManager &OM): 
      OperandTable(OM) {
      //OM.printAll(std::cout);
    }    
    
    // * Translating patterns to LLVM's instruction selection phase *
    // Header
    std::string genEmitSDNodeHeader(unsigned FuncID);
    // Code
    std::string genEmitSDNode(SearchResult *SR, const std::string& L,
			         unsigned FuncID, LiteralMap* LMap);
    void genEmitSDNode(SDNode* N, SDNode* LLVMDAG, 
			  const std::list<unsigned>& pathToNode,
			  const OpTransLists* OTL,
		       std::ostream &S,
		       const std::list<unsigned>* Flag = 0,
		       bool createOutFlag = false);
    // Matcher
    void genSDNodeMatcher(const std::string &LLVMDAG, 
			 std::map<std::string, MatcherCode> &Map,
			 const std::string &EmitFuncName);
			 
    // * Translating patterns to LLVM's instruction scheduling phase *
    std::string genEmitMI(SearchResult *SR, const StringMap& Defs,
			  LiteralMap *LMap,
			  bool alreadySorted = false,
			  bool mayUseVirtualReg = true,
			  std::list<const Register*>* auxiliarRegs = NULL,
			  unsigned ident = 2, 
			  const StringMap* ExtraParams = NULL,
			  const std::string& MBB = "MBB",
			  const std::string& Itr = "I",
			  const std::string& get = "get");
			  
    std::string genIdentifyMI(SearchResult *SR, const StringMap& Defs,
			      LiteralMap *LMap,
			      const std::string &Action,
			      const std::string &MI = "MI",
			      unsigned ident = 2,
			      bool ErasePatt = false);
    std::string genFindMIPatRoot(SearchResult *SR, 
																 const std::string& InitialMI,
																 const std::string& OperandName,					   
																 unsigned ident = 2);
  };

		// These classes are used to organize the utilization of the small set of 
		// auxiliary registers in genEmitMI(), using a simple register allocation
		// algorithm.
		class RegAllocIt {
				const Register* physReg;
				std::string owner;  
		public:
				RegAllocIt(const Register* pr): physReg(pr), owner("FREE") {}
				void SetOwner(const std::string& N) {
						owner = N;
				}
				const std::string& Owner() {
						return owner;
				}
				const std::string& PhysReg() {
						return physReg->getName();
				}
				const Register* getPhysReg() {
						return physReg;
				}
		};
		struct InsufficientAuxiliarRegsException{};
		// A list of RegAllocItems
		class RegAlloc : public std::list<RegAllocIt> {
				// Keep track of current usable register
				iterator CurUsableReg;
				// Keep track of current instruction operands to analyze defs/uses
				OperandsDefsType::const_iterator CurInsOperands, InsEnd;
		public:  
				static RegAlloc* Build(const std::list<const Register*>* AList, 
															 OperandsDefsType::const_iterator FirstInsn,
															 OperandsDefsType::const_iterator LastInsn) {
						RegAlloc* RA = new RegAlloc();
						for (std::list<const Register*>::const_iterator I = AList->begin(), 
										 E = AList->end(); I != E; ++I) {
								RA->push_back(RegAllocIt(*I));
						}
						RA->CurUsableReg = RA->begin();
						RA->CurInsOperands = FirstInsn;
						RA->InsEnd = LastInsn;
						return RA;
				}
				// Tell the allocator that we are now analyzing the next instructions
				// in the list. If we arrived at the end of the list, automatically
				// deallocate itself, as its task has ended.
				void NextInstruction() {
						++CurInsOperands;
						if (CurInsOperands == InsEnd) {
								delete this;
						}
				}
				bool IsLive(const std::string &VirtReg) {
						OperandsDefsType::const_iterator I = CurInsOperands, E = InsEnd;
						++I;
						for (;I != E; ++I) {
								NameListType* OpNames = *I;
								for (NameListType::const_iterator NI = OpNames->begin(),
												 NE = OpNames->end(); NI != NE; ++NI) {
										if (*NI == VirtReg)
												return true;
								}
						}
						return false;
				}
				const std::string& getPhysRegister(const std::string &VirtReg) {
						// First perform a lookup to see if this virtreg is already allocated
						for (iterator I = begin(), E = end(); I != E; ++I) {
								if (I->Owner() == VirtReg)
										return I->PhysReg();
						}
						// Need to allocate a new phys reg. to it
						for (iterator I = begin(), E = end(); I != E; ++I) {
								if (I->Owner() == "FREE" || !IsLive(I->Owner())) {
										I->SetOwner(VirtReg);
										return I->PhysReg();
								}
						}
						throw InsufficientAuxiliarRegsException();
				}
				const Register* getPhysRegisterRef(const std::string &VirtReg) {
						// First perform a lookup to see if this virtreg is already allocated
						for (iterator I = begin(), E = end(); I != E; ++I) {
								if (I->Owner() == VirtReg)
										return I->getPhysReg();
						}
						// Need to allocate a new phys reg. to it
						for (iterator I = begin(), E = end(); I != E; ++I) {
								if (I->Owner() == "FREE" || !IsLive(I->Owner())) {
										I->SetOwner(VirtReg);
										return I->getPhysReg();
								}
						}
						throw InsufficientAuxiliarRegsException();
				}

		};


}



#endif
