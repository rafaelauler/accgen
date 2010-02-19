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


namespace backendgen {  

  namespace expression { 
    class Tree;
  }

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
  private:
    std::list<Rule> Rules;
  };


  

}
