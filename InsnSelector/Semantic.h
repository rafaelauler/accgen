//===- Semantic.h - Header file for Semantic related class--*- C++ -*------===//
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

  // Expression namespace encapsulates all classes related to
  // expression trees (the tree itself and its nodes).
  namespace expression {

    // An expression tree node.
    class Node {
    };

    // An expression tree, representing a single assertion of an
    // instruction semantics over the processor state.    
    typedef Tree Node;

    // We may have several operand types, albeit some predefined
    // values exist.
    typedef int OperandType;

    // Each expression operand must have a known type. This type
    // may be known (hardcoded types) or given by the user. This
    // is the operand type table with all existing values.
    class OperandTableManager {
    private:
      std::map<std::string, OperandType> TypeMap;
    };
       
    // An operand node is a necessarily leaf node, representing
    // some class of storage in the processor.
    class Operand : Node {
    private:
      int Size;
      OperandType Type;
    };

    // A specialization of operand: Constant operands are known
    // and do not represent some kind of storage, but has type
    // information.
    class Constant : Operand {
    private:
      int Value;
    };

    // Operator types' interface.
    // One different OperatorTypes may exist for each type defined 
    // by the user.
    struct OperatorType {
      int Arity = 0;
      int Type = 0;
    };

    // Manages all existing operator types in a table.
    class OperatorTableManager {
    private:
      std::map<std::string, OperatorType> OperatorMap;
      int CurrentIndex = 0;
    };
    
    // An operator node is a necessarily non-leaf node which 
    // does some computation based on its operands. The number
    // of operands is determined by its arity.
    class Operator : Node {
    public:
      // Constructor
      Operator(OperatorType OpType): Type(OpType), Children(OpType.Arity) {	
	super();       
      }
      // Subscripting directly access one of this node's children.
      Node*& operator[] (int index) {
	return Children[index];
      }
    private:
      std::vector<Node *> Children;
      OperatorType Type;
    };  

  } // end namespace expression
  
  // Contains a colletion of expression trees, representing assertions
  // on storage elements of the processor. These encode insn. behaviour.
  class Semantic {
  private:
    std::list<expression::Tree *> ExpressionTrees;
  };


  

} // end namespace backendgen
