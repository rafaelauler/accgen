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
    unsigned NumOperands;
    SDNode** ops;
    SDNode();
    ~SDNode();
    void SetOperands(unsigned num);
    void Print(std::ostream &S);
  };

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
  public:
    SDNode *generateInsnsDAG(SearchResult *SR);
  };

}



#endif
