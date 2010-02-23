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
    // If this rule can be applied to decompose
    bool Decomposition;
    // If applying the inverse of this rule can be used to decompose
    bool Composition;
    Rule(expression::Tree* LHS, expression::Tree* RHS, bool Equivalence);
    std::list<expression::Tree*>* Decompose(const expression::Tree* Expression)
    const;
    expression::Tree* ForwardApply(const expression::Tree* Expression) const;
    expression::Tree* BackwardApply(const expression::Tree* Expression) const;
  };

  typedef std::list<Rule>::const_iterator RuleIterator;

  class TransformationRules {
  public:
    bool createRule(expression::Tree* LHS, expression::Tree* RHS,
		    bool Equivalence);
    void print();
    ~TransformationRules();
    RuleIterator getBegin();
    RuleIterator getEnd();
  private:
    std::list<Rule> Rules;
  };


  

}

#endif
