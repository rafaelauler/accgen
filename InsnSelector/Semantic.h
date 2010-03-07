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
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <set>

namespace backendgen {

  // Expression namespace encapsulates all classes related to
  // expression trees (the tree itself and its nodes).
  namespace expression {
    
    // An expression tree node.
    class Node {
    public:
      virtual void print(std::ostream& S) const { S << "GenericNode";  }
      virtual ~Node() { }
      virtual bool isOperand() const { return false; }
      virtual bool isOperator() const { return false; }
      virtual unsigned getType() const { return 0; }
      virtual unsigned getSize() const { return 0; }
      virtual Node* clone() const { return new Node(*this); }
    };

    // An expression tree, representing a single assertion of an
    // instruction semantics over the processor state.    
    typedef Node Tree;

    // We may have several operand types, albeit some predefined
    // values exist.
    enum KnownTypes {IntType=1, LastType};

    const std::string IntTypeStr = "int";

    struct OperandType {
      unsigned int Type;
      unsigned int Size;
      unsigned int DataType;
      OperandType(unsigned Type, unsigned Size, unsigned DataType) :
	Type(Type), Size(Size), DataType(DataType) {}
      OperandType() {}
      bool operator< (const OperandType &a) const {
	return (Type < a.Type);
      }
    };
      
    typedef std::map<std::string, OperandType> TypeMapType;
    typedef std::map<OperandType, std::string> ReverseTypeMapType;
    
    // Each expression operand must have a known type. This type
    // may be known (hardcoded types) or given by the user. This
    // is the operand type table with all existing values.
    class OperandTableManager {
    public:
      OperandType getType(const std::string &Name);
      int updateSize (OperandType Type, unsigned int NewSize);
      void setCompatible (const std::string &O1, const std::string &O2);
      void printAll (std::ostream& S) const;
    private:
      TypeMapType TypeMap;
      ReverseTypeMapType ReverseTypeMap;
    };
       
    // An operand node is a necessarily leaf node, representing
    // some class of storage in the processor.
    class Operand : public Node {
    public:
      Operand (const OperandType &Type, const std::string &OpName);
      virtual void print(std::ostream &S) const {	
	S << "Operand" << Type.Type;  
      }
      virtual bool isOperand() const { return true; }
      virtual unsigned getType() const { return Type.Type; }
      virtual unsigned getSize() const { return Type.Size; }
      virtual Node* clone() const { return new Operand(*this); }
      const std::string& getOperandName() const {return OperandName;}
      unsigned getDataType() const { return Type.DataType; }
      void changeOperandName(const std::string &NewName) {
	OperandName = NewName;
      }
    protected:      
      OperandType Type;
      std::string OperandName;
    };

    typedef unsigned int ConstType;

    // A specialization of operand: Constant operands are known
    // and do not represent some kind of storage, but has type
    // information.
    class Constant : public Operand {
    public:
      Constant (const ConstType Val, const OperandType &Type);
      virtual void print(std::ostream &S) const { S << Value;  }
      virtual Node* clone() const { return new Constant(*this); }
    private:
      ConstType Value;
    };

    // Defines a register
    class Register {
    public:
      Register(const std::string &RegName);
      void addSubClass(Register *Reg);
      const std::string &getName() const;
      std::list<Register*>::const_iterator getSubsBegin() const;
      std::list<Register*>::const_iterator getSubsEnd() const;
    private:
      std::string Name;
      std::list<Register*> SubClasses;      
    };
    
    // Defines a class of uniform registers
    class RegisterClass {
    public:
      RegisterClass(const std::string &ClassName);
      RegisterClass(const std::string &ClassName, const OperandType &OpType);
      bool addRegister(Register *Reg);
      bool hasRegister(Register *Reg);
      const std::string &getName() const;
      OperandType getOperandType() const;
      std::set<Register*>::const_iterator getBegin() const;
      std::set<Register*>::const_iterator getEnd() const;
    private:
      std::set<Register *> Registers;    
      std::string Name;
      OperandType Type;
    };

    // Defines a calling convention for a specific operand type.
    // It has a list of what registers (or stack) can be used to
    // send or return the parameter of this type to a function.
    class CallingConvention {
      std::list<const Register*> Regs;
    public:
      // If not return convention, calling convention is assumed.
      bool IsReturnConvention;
      bool UseStack;
      unsigned StackSize;
      unsigned StackAlign;
      OperandType Type;
      CallingConvention(bool IsReturnConvention, bool UseStack) : 
	IsReturnConvention(IsReturnConvention), UseStack(UseStack) {}
      void addRegister(const Register* Elem) {Regs.push_front(Elem);}
      std::list<const Register*>::const_iterator getBegin() const {
	return Regs.begin();
      }
      std::list<const Register*>::const_iterator getEnd() const {
	return Regs.end();
      }
    };

    // Defines a register class manager
    class RegClassManager {
    public:
      ~RegClassManager();
      bool addRegClass(RegisterClass *RegClass);
      bool addRegister(Register *Reg);
      bool addCalleeSaveRegister(Register *Reg);
      bool addReservedRegister(Register *Reg);
      void addCallingConvention(CallingConvention* Elem);
      RegisterClass *getRegClass(const std::string &ClassName);
      Register *getRegister(const std::string &RegisterName);
      RegisterClass *getRegRegClass(Register* Reg);
      std::set<RegisterClass*>::const_iterator getBegin() const;
      std::set<RegisterClass*>::const_iterator getEnd() const;
      std::set<Register*>::const_iterator getRegsBegin() const;
      std::set<Register*>::const_iterator getRegsEnd() const;
      std::set<Register*>::const_iterator getReservedBegin() const;
      std::set<Register*>::const_iterator getReservedEnd() const;
      std::set<Register*>::const_iterator getCalleeSBegin() const;
      std::set<Register*>::const_iterator getCalleeSEnd() const;
      std::list<CallingConvention*>::const_iterator getCCBegin() const;
      std::list<CallingConvention*>::const_iterator getCCEnd() const;
    private:
      std::set<RegisterClass *> RegClasses;
      std::set<Register *> Registers;
      std::set<Register *> CalleeSaveRegisters;
      std::set<Register *> ReservedRegisters;
      std::list<CallingConvention*> CallConvs;
    };

    // Special class of operand:  a register class
    class RegisterOperand : public Operand {
      const RegisterClass *MyRegClass;
    public:
      RegisterOperand (const RegisterClass *RegClass,
		       const std::string &OpName);
      virtual void print(std::ostream &S)  const { 
	S << MyRegClass->getName() << ":" << Type.Type;
      };
      virtual Node* clone() const { return new RegisterOperand(*this); }
      const RegisterClass *getRegisterClass() const;
    };


    // Another specialization of operand: an immediate
    // We need to explicitly identify immediates, so we know when
    // we can fiddle directly with these bits
    class ImmediateOperand : public Operand {
    public:
      ImmediateOperand (const OperandType &Type, const std::string &OpName);
      virtual Node* clone() const { return new ImmediateOperand(*this); }
    };

    // Encodes special (specific) operators that we must know in order to
    // perform some transformations
    enum KnownOperators {AddOp=1, SubOp, DecompOp, IfOp, AssignOp, LastOp};

    const std::string AddOpStr = "+";
    const std::string SubOpStr = "-";
    const std::string DecompOpStr = "dec";        
    const std::string IfOpStr = "if";
    const std::string AssignOpStr = "transfer";

    // Operator types' interface.
    // One different OperatorTypes may exist for each type defined 
    // by the user.
    struct OperatorType {
      int Arity;
      int Type;
      bool operator< (const OperatorType &a) const {
	return (Type < a.Type);
      }
    };

    typedef std::map<std::string, OperatorType> OperatorMapType;
    typedef std::map<OperatorType, std::string> ReverseOperatorMapType;

    // Manages all existing operator types in a table.
    class OperatorTableManager {
    public:
      OperatorType getType (const std::string &Type);
      void setAlias (const std::string &Op1, const std::string &Op2);
      int updateArity (OperatorType Type, int NewArity);
      std::string getOperatorName (OperatorType type);
    private:
      OperatorMapType OperatorMap;
      ReverseOperatorMapType ReverseOperatorMap;
    };
    
    // An operator node is a necessarily non-leaf node which 
    // does some computation based on its operands. The number
    // of operands is determined by its arity.   
    class Operator : public Node {
    public:
      // Constructor
      Operator(OperatorType OpType): Node(), Children(OpType.Arity),
				     Type(OpType)
      {
	ReturnType.Type = 0;
	ReturnType.Size = 0;
	ReturnType.DataType = 0;
      }
      // Subscripting directly access one of this node's children.
      Node*& operator[] (int index) {
	//if (index >= Type.Arity)
	//std::cerr << "Danger: accessing unexisting element\n";	
	return Children[index];
      }
      Node* operator[] (int index) const {
	//if (index >= Type.Arity)
	//std::cerr << "Danger: accessing unexisting element\n";	
	return Children[index];
      }
      virtual void print(std::ostream &S) const {
	S << "(Operator" << Type.Type << "Arity" << Type.Arity << " ";
	for (unsigned I = 0, E = Type.Arity; I < E; ++I) 
	  {
	    Children[I]->print(S);
	    S << " ";
	  }	
	S << ") ";
      }
      virtual bool isOperator() const { return true; }
      virtual unsigned getType() const { return (unsigned) Type.Type; }
      virtual unsigned getSize() const { return ReturnType.Size; }
      virtual Node* clone() const { 
	Operator* Pointer = new Operator(*this);
	for (unsigned I = 0, E = Type.Arity; I < E; ++I) 
	  {
	    (*Pointer)[I] = Children[I]->clone();
	  }	
	return Pointer; 
      }
      virtual ~Operator() {
	for (unsigned I = 0, E = Type.Arity; I < E; ++I) 
	  {
	    if (Children[I] != NULL)
	      delete Children[I];
	  }
      }
      void detachNode() { 
	for (unsigned I = 0, E = Type.Arity; I < E; ++I) 
	  {	    
	    Children[I] = NULL;
	  }
      }
      void setReturnType (const OperandType &OType);
      unsigned getReturnTypeSize() const {return ReturnType.Size;}
      unsigned getReturnTypeType() const {return ReturnType.Type;}
      int getArity() const {return Type.Arity;}
      OperatorType getOpType() const {return Type;}
    private:
      std::vector<Node *> Children;
      OperatorType Type;
      OperandType ReturnType;
    };  

  } // end namespace expression
  

} // end namespace backendgen

#endif

