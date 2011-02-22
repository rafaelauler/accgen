//                    -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode nil-*-
//===- Semantic.h - Header file for Semantic related class-----------------===//
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
#include <cassert>

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
      virtual Node* clone() const = 0;
      virtual unsigned getHash(unsigned hash_chain) const =  0;
      virtual unsigned getHash() const { return getHash(0); }
      virtual bool isTransferDestination() const = 0;
      virtual void setIsTransferDestination(bool NewVal) = 0;
    };

    // An expression tree, representing a single assertion of an
    // instruction semantics over the processor state.    
    typedef Node Tree;

  }

  typedef std::pair<std::string, std::string> ABinding;
  typedef std::list<ABinding> BindingsList;

  struct Semantic {
    expression::Tree* SemanticExpression;
    BindingsList* OperandsBindings;
    Semantic() {
      SemanticExpression = NULL;
      OperandsBindings = NULL;
    }
  };

  namespace expression {    

    // We may have several operand types, albeit some predefined
    // values exist.
    enum KnownTypes {IntType=1, CondType, LastType};

    const std::string IntTypeStr = "int";
    const std::string CondTypeStr = "cond";
    
    // Cond values
    const std::string CondLtStr = "lt";
    const std::string CondGtStr = "gt";
    const std::string CondLeStr = "le";
    const std::string CondGeStr = "ge";
    const std::string CondUltStr = "ult";
    const std::string CondUgtStr = "ugt";
    const std::string CondUleStr = "ule";
    const std::string CondUgeStr = "uge";
    const std::string CondNeStr = "ne";
    const std::string CondEqStr = "eq";
    
    enum CondValues {LtCondVal=1, GtCondVal, LeCondVal, GeCondVal, UltCondVal,
		     UgtCondVal, UleCondVal, UgeCondVal, NeCondVal, EqCondVal};

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
    typedef unsigned int ConstType;
    
    // Each expression operand must have a known type. This type
    // may be known (hardcoded types) or given by the user. This
    // is the operand type table with all existing values.
    class OperandTableManager {
    public:
      OperandType getType(const std::string &Name);
      const std::string& getTypeName(const OperandType &OpType);
      int updateSize (OperandType Type, unsigned int NewSize);
      void setCompatible (const std::string &O1, const std::string &O2);
      void printAll (std::ostream& S) const;
      static ConstType parseCondVal (const std::string &S);
    private:
      TypeMapType TypeMap;
      ReverseTypeMapType ReverseTypeMap;
    };
       
    // An operand node is a necessarily leaf node, representing
    // some class of storage in the processor.
    class Operand : public Node {
    public:
      Operand (OperandTableManager &Man, const OperandType &Type,
	       const std::string &OpName): Manager(Man) {
	this->Type = Type;
	OperandName = OpName;
	SpecificReference = false;
	AcceptsSpecificReference = false;
	IsTransferDestination = false;
      }
      virtual void print(std::ostream &S) const {	
	S << OperandName << ":" << Manager.getTypeName(Type);  
	if (IsTransferDestination)
	  S << "*";
      }
      virtual bool isOperand() const { return true; }
      virtual unsigned getType() const { return Type.Type; }
      virtual unsigned getSize() const {
	return Type.Size;
      }
      virtual Node* clone() const { return new Operand(*this); }
      virtual unsigned getHash(unsigned hash_chain) const;
      virtual bool isTransferDestination() const {
	return IsTransferDestination; 
      }
      virtual void setIsTransferDestination(bool NewVal) {
	IsTransferDestination = NewVal;
      }
      void updateSize() {
	Type = Manager.getType(Manager.getTypeName(Type));
      }
      const std::string& getOperandName() const {return OperandName;}
      unsigned getDataType() const { return Type.DataType; }
      bool isSpecificReference() const { return SpecificReference; }
      void setSpecificReference(bool Val) {SpecificReference = Val; }
      void setAcceptsSpecificReference(bool Val) {
	AcceptsSpecificReference = Val;
      }
      bool acceptsSpecificReference() const {
	return AcceptsSpecificReference;
      }
      void changeOperandName(const std::string &NewName) {
	OperandName = NewName;
      }
    protected:      
      OperandType Type;
      std::string OperandName;
      // The operand name bounds to an assembly operand, in which
      // case the exact reference is determined by the assembly construct, OR 
      // it refers directly to a specific register in the architecture.
      bool SpecificReference;
      bool AcceptsSpecificReference;
      bool IsTransferDestination;
      OperandTableManager &Manager;
    };

    // A FragOperand must be expanded by inserting a semantic
    // fragment in its place
    class FragOperand : public Operand {      
      std::list<std::string> ParameterList;
    public:
      FragOperand(OperandTableManager &Man, const std::string &OpName,
		  std::list<std::string>&List): 
	Operand(Man, OperandType(), OpName), ParameterList(List) {}
      std::list<std::string>& getParameterList() { return ParameterList; }
    };    

    // A specialization of operand: Constant operands are known
    // and do not represent some kind of storage, but has type
    // information.
    class Constant : public Operand {
    public:
      Constant (OperandTableManager &Man, const ConstType Val,
		const OperandType &Type);
      virtual void print(std::ostream &S) const { 
	S << "const:" << Value << ":" << Manager.getTypeName(Type); 
      }
      virtual Node* clone() const { return new Constant(*this); }
      ConstType getConstValue() const { return Value; }
    private:
      ConstType Value;
      static unsigned SeqNum;
    };

    // Defines a register
    class Register {
    public:
      typedef std::list<Register*>::iterator Iterator;
      typedef std::list<Register*>::const_iterator ConstIterator;

      Register(const std::string &RegName);
      void addSubClass(Register *Reg);
      const std::string &getName() const;
      const std::string &getAssemblyName() const;
      ConstIterator getSubsBegin() const;
      ConstIterator getSubsEnd() const;
    private:
      void buildName();
      std::string AssemblyName, Name;
      std::list<Register*> SubClasses;      
    };
    
    // Defines a class of uniform registers
    class RegisterClass {
    public:
      typedef std::set<Register*>::iterator Iterator;
      typedef std::set<Register*>::const_iterator ConstIterator;

      RegisterClass(const std::string &ClassName);
      RegisterClass(const std::string &ClassName, const OperandType &OpType);
      bool addRegister(Register *Reg);
      bool hasRegister(const Register *Reg);
      bool hasRegisterName(const std::string &RegName);
      const std::string &getName() const;
      OperandType getOperandType() const;
      ConstIterator getBegin() const;
      ConstIterator getEnd() const;
      const Register* getRegAt(unsigned i) const;
      unsigned getNumRegs() const {return Registers.size();}
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
      RegClassManager();
      ~RegClassManager();
      bool addRegClass(RegisterClass *RegClass);
      bool addRegister(Register *Reg);
      bool addCalleeSaveRegister(Register *Reg);
      bool addReservedRegister(Register *Reg);
      bool addAuxiliarRegister(Register *Reg);
      void addCallingConvention(CallingConvention* Elem);
      void setProgramCounter(const Register* Reg);
      void setReturnRegister(const Register* Reg);
      // Stack member functions
      void setFramePointer(const Register* Reg);
      void setStackPointer(const Register* Reg);
      void setGrowsUp(bool B) {
	GrowsUp = B;
      }
      void setAlignment(unsigned A) {
	Alignment = A;
      }      
      // ---
      void setPCOffset(int A) {
	PCOffset = A;
      }
      RegisterClass *getRegClass(const std::string &ClassName);
      Register *getRegister(const std::string &RegisterName);
      RegisterClass *getRegRegClass(const Register* Reg);      
      std::set<RegisterClass*>::const_iterator getBegin() const;
      std::set<RegisterClass*>::const_iterator getEnd() const;
      std::set<Register*>::const_iterator getRegsBegin() const;
      std::set<Register*>::const_iterator getRegsEnd() const;
      std::set<Register*>::const_iterator getReservedBegin() const;
      std::set<Register*>::const_iterator getReservedEnd() const;
      std::set<Register*>::const_iterator getAuxiliarBegin() const;
      std::set<Register*>::const_iterator getAuxiliarEnd() const;
      std::set<Register*>::const_iterator getCalleeSBegin() const;
      std::set<Register*>::const_iterator getCalleeSEnd() const;
      std::list<const Register*>* getCallerSavedRegs() const;
      std::list<CallingConvention*>::const_iterator getCCBegin() const;
      std::list<CallingConvention*>::const_iterator getCCEnd() const;
      // Stack member functions
      const Register *getProgramCounter() const;
      const Register *getReturnRegister() const;
      const Register *getFramePointer() const;
      const Register *getStackPointer() const;
      int getPCOffset() const { return PCOffset; }
      unsigned getAlignment() const { return Alignment; }
      bool getGrowsUp() const { return GrowsUp; }
      // ---
    private:
      std::set<RegisterClass *> RegClasses;
      std::set<Register *> Registers;
      std::set<Register *> CalleeSaveRegisters;
      std::set<Register *> ReservedRegisters;
      std::set<Register *> AuxiliarRegisters;
      std::list<CallingConvention*> CallConvs;
      const Register* ProgramCounter;
      const Register* ReturnRegister;
      // Stack information
      const Register* StackPointer;
      const Register* FramePointer;
      unsigned Alignment;
      int PCOffset;
      bool GrowsUp;
    };

    // This class stores and manages fragments of instruction semantic, used
    // to factor out common parts used in more than one semantic tree.
    // It must also apply these fragments to build the complete
    // instruction semantic tree when necessary.
    class FragmentManager {
      std::map<std::string, std::vector<Tree*>*> FragMap;
    public:      
      ~FragmentManager();

      // Iterators
      typedef std::map<std::string, std::vector<Tree*>*>::iterator Iterator;
      typedef std::vector<Tree*>::iterator VecIterator;

      bool addFragment(const std::string &Name, Tree* Frag);
      Iterator getBegin();
      Iterator getEnd();
      Iterator findFrag(const std::string &Name);
      bool expandTree(Tree** Semantic);
    };

    // Special class of operand:  a register class
    class RegisterOperand : public Operand {
      const RegisterClass *MyRegClass;
    public:
      RegisterOperand (OperandTableManager& Man, const RegisterClass *RegClass,
		       const std::string &OpName);
      virtual void print(std::ostream &S)  const { 
	S << OperandName << ":" << MyRegClass->getName() << ":" 
	  << Manager.getTypeName(Type);
	if (IsTransferDestination)
	  S << "*";
      };
      virtual Node* clone() const { return new RegisterOperand(*this); }
      const RegisterClass *getRegisterClass() const;
    };


    // Another specialization of operand: an immediate
    // We need to explicitly identify immediates, so we know when
    // we can fiddle directly with these bits
    class ImmediateOperand : public Operand {
    public:
      ImmediateOperand (OperandTableManager &Man, const OperandType &Type,
			const std::string &OpName);
      virtual void print(std::ostream &S) const { 
	S << "imm:" << OperandName << ":" << Manager.getTypeName(Type); 
      }
      virtual Node* clone() const { return new ImmediateOperand(*this); }
    };

    // Encodes special (specific) operators that we must know in order to
    // perform some transformations
    enum KnownOperators {AddOp=1, SubOp, NegOp, DecompOp, IfOp, AssignOp,
			 MemRefOp, CallOp, JumpOp, CJumpOp, ReturnOp, LastOp};

    const std::string AddOpStr = "+";
    const std::string SubOpStr = "-";
    const std::string NegOpStr = "~";
    const std::string DecompOpStr = "dec";        
    const std::string IfOpStr = "if";
    const std::string AssignOpStr = "transfer";
    const std::string MemRefOpStr = "memref";
    const std::string CallOpStr = "call";
    const std::string JumpOpStr = "jump";
    const std::string CJumpOpStr = "cjump";
    const std::string ReturnOpStr = "ret";

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
    protected:
      // Constructor
      Operator(OperatorTableManager &Man, OperatorType OpType):
        Node(), Children(OpType.Arity), Type(OpType), Manager(Man)
      {
	ReturnType.Type = 0;
	ReturnType.Size = 0;
	ReturnType.DataType = 0;
	IsTransferDestination = false;
      }
    public:
      // Operator factory
      static Operator* BuildOperator(OperatorTableManager &Man,
				     OperatorType OpType);
      // Subscripting directly access one of this node's children.
      //Node*& operator[] (int index) {
	//if (index >= Type.Arity)
	//std::cerr << "Danger: accessing unexisting element\n";
	//if (Type.Type == AssignOp && index == 0)
	//  std::cerr << "Teste.\n";
	//assert ((Type.Type != AssignOp || index != 0)
	//        && "Illegal use of operator[] to change transfer destination");
	//return Children[index];
      //}      
      Node* operator[] (int index) const {
	//if (index >= Type.Arity)
	//std::cerr << "Danger: accessing unexisting element\n";	
	return Children[index];
      }
      bool isAssignOp() const {
	return Type.Type == AssignOp;
      }
      void setChild(int index, Node *N) {
	if (Type.Type == AssignOp && index == 0) {
	  //if (Children[0] != 0) 
	    //Children[0]->setIsTransferDestination(false);	
	  if (N != 0)
	    N->setIsTransferDestination(true);
	  Children[0] = N;
	  return;
	}
	Children[index] = N;
      }
      virtual void print(std::ostream &S) const {
	S << "(" << Manager.getOperatorName(Type) <<  " ";
	for (unsigned I = 0, E = Type.Arity; I < E; ++I) 
	  {
	    Children[I]->print(S);
	    S << " ";
	  }	
	if (IsTransferDestination)
	  S << ")* ";
	else
	  S << ") ";
      }
      virtual bool isOperator() const { return true; }
      virtual unsigned getType() const { return (unsigned) Type.Type; }
      virtual unsigned getSize() const { return ReturnType.Size; }
      virtual bool isTransferDestination() const {
	return IsTransferDestination; 
      }
      virtual void setIsTransferDestination(bool NewVal) {
	IsTransferDestination = NewVal;
      }
      virtual Node* clone() const { 
	Operator* Pointer = new Operator(*this);
	for (unsigned I = 0, E = Type.Arity; I < E; ++I) 
	  {
	    Pointer->setChild(I, Children[I]->clone());
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
      virtual unsigned getHash(unsigned hash_chain) const;
      void detachNode() { 
	if (Type.Type == AssignOp && Children[0] != 0)
	  Children[0]->setIsTransferDestination(false);	  
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
    protected:
      std::vector<Node *> Children;
      OperatorType Type;
      OperandType ReturnType;
      OperatorTableManager &Manager;
      bool IsTransferDestination;
    };     

    struct PatternElement {
      std::string Name;
      std::string LLVMDAG;
      const Tree* TargetImpl;
      PatternElement(std::string n, std::string d, const Tree* ti) :
	  Name(n), LLVMDAG(d), TargetImpl(ti) {}
    };

    typedef std::list<PatternElement> PatternList;

    class PatternManager {
      PatternList PatList;
    public:
      typedef PatternList::iterator Iterator;
      ~PatternManager();
      void AddPattern(std::string Name, std::string LLVMDAG,
		      const Tree* TargetImpl);
      Iterator begin();
      Iterator end();
      unsigned size() const {
	return PatList.size();
      }
      Iterator getElementAt(unsigned n) {
	Iterator I = begin();
	for (unsigned cur = 0; cur < n; ++cur) {
	  ++I;
	}
	return I;
      }
      
      // Static utils - Pattern Generation
      static const Tree* genCopyRegToRegPat(OperatorTableManager& OpMan,
					    OperandTableManager& OM,
	RegClassManager& Man,
	const std::string& DestRC, const std::string& Dest,
	const std::string& SrcRC, const std::string& Src);
      static const Tree* 
      genCopyAddSubImmPat(OperatorTableManager& OpMan,
			  OperandTableManager& OM,
			  RegClassManager& RegMan,
			  bool isAddition,
			  const std::string& DestRC,
			  const std::string& Dest,
			  const std::string& SrcRC,
			  const std::string& Src,
			  const std::string& Imm);
      static const Tree*
      genNegRegPat(OperatorTableManager& OpMan,
		   OperandTableManager& OM,
		   RegClassManager& RegMan,
		   const std::string& DestRC,
		   const std::string& Dest,
		   const std::string& Imm);
    };

  } // end namespace expression
  

} // end namespace backendgen

#endif

