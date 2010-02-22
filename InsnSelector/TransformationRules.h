//===- TransformationRules.h                                    --*- C++ -*===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// These classes represent the instruction behaviour, as perceived by the
// compiler backend generator tool.
//
//===----------------------------------------------------------------------===//
#ifndef TRANSFORMATIONRULES_H
#define TRANSFORMATIONRULES_H

#include "Semantic.h"
#include <list>

namespace backendgen {  

  // A Rule represents a given transformation
  struct Rule {  
    // References to left hand side and right hand side expression trees.
    expression::Tree* LHS; 
    expression::Tree* RHS;
    // Establishes if the given rule may be applied in reverse fashion.
    // (RHS derives LHS).
    bool Equivalence;
  };

  class TransformationRules {
  public:
    bool createRule(expression::Tree* LHS, expression::Tree* RHS,
		    bool Equivalence);
    void print();
    ~TransformationRules();
  private:
    std::list<Rule> Rules;
  };


  

}

#endif
