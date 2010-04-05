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

//===-- Class prototypes ---===//
namespace backendgen {
  class SDNode {
  public:
    std::string ;
    SDNode* ops;
  };

  class PatternTranslator {
  public:
    std::string generateInsnsDAG(SearchResult *SR);
  };

}



#endif
