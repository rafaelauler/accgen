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
    bool IsLiteral; 
    // If specific register, we need to print it differently, as LLVM will
    // already known this name and recognize as a register.
    bool IsSpecificReg;
                                 
    unsigned NumOperands;
    SDNode** ops;
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
  
  struct PTParseErrorException {};  

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
  public:
    PatternTranslator(OperandTableManager &OM): 
      OperandTable(OM) {
      //OM.printAll(std::cout);
    }
    void sortOperandsDefs(NameListType* OpNames, SemanticIterator SI);
    SDNode *generateInsnsDAG(SearchResult *SR);
    SDNode *parseLLVMDAGString(const std::string &S);
    SDNode *parseLLVMDAGString(const std::string &S, unsigned *pos);
    std::string generateEmitHeader(unsigned FuncID);
    std::string generateEmitCode(SearchResult *SR, const std::string& L,
			         unsigned FuncID);
    void generateEmitCode(SDNode* N, SDNode* LLVMDAG, unsigned level,
			  unsigned cur, std::ostream &S);
    void generateMatcher(const std::string &LLVMDAG, std::map<std::string, MatcherCode> &Map,
			 const std::string &EmitFuncName);
  };

}



#endif
