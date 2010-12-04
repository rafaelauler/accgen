//===- LLVMDAGInfo.cpp - ------------------------------------*- C++ -*-----===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// This file contains all LLVM's SelectionDAG information necessary to generate
// the matcher code. For example, we need to know what is the ISD enumeration
// name of a specific node. In the future, this information ought to be parsed
// from tablegen files in order to the backend generator be more flexible when
// the LLVM target independent code generator gets updated.
//
//===----------------------------------------------------------------------===//

#include "LLVMDAGInfo.h"
#include "InsnSelector/Semantic.h"
#include <iostream>
#include <sstream>
#include <cassert>

using namespace LLVMDAGInfo;
using std::stringstream;
using std::string;
using std::endl;
using std::map;

// Defined in Parser
extern backendgen::expression::OperandTableManager OperandTable;

namespace {
  void
  HandleOTL(string& Operand, list<const OperandTransformation*>* OTL)
  {
    assert (OTL != NULL);
    for (list<const OperandTransformation*>::const_iterator I = OTL->begin(),
      E = OTL->end(); I != E; ++I) 
    {
      Operand = (*I)->PatchTransformExpression(Operand);
      Operand.append(1, ')');
      Operand.insert((string::size_type)0,1,'(');
    }
  }
  
  string
  GetFrameIndex(const string &N, list<const OperandTransformation*>* OTL) 
  {
    stringstream SS, SSOperand;    
    SSOperand << "dyn_cast<FrameIndexSDNode>(" 
	      << N
	      << ")->getIndex()";
    string Operand = SSOperand.str();
    if (OTL) {
      HandleOTL(Operand, OTL);
    }
    SS << "CurDAG->getTargetFrameIndex(" << Operand << ", TLI.getPointerTy());"
       << endl;    
    return SS.str();
  }
  
  string
  GetConstant(const string &N, list<const OperandTransformation*>* OTL)
  {
    stringstream SS, SSOperand;    
    SSOperand << "((unsigned) cast<ConstantSDNode>(" 
	      << N
	      << ")->getZExtValue())";
    string Operand = SSOperand.str();
    if (OTL)
      HandleOTL(Operand, OTL);
    SS << "CurDAG->getTargetConstant(" << Operand << ", MVT::i32);"
       << endl;    
    return SS.str();
  }
  
  string
  GetGlobalAddress(const string &N, list<const OperandTransformation*>* OTL)
  {
    stringstream SS, SSOperand;        
    SSOperand << "cast<GlobalAddressSDNode>(" 
	      << N
	      << ")->getGlobal()";
    string Operand = SSOperand.str();
    assert (OTL == NULL && "GlobalAddress should not be OperandTransform'ed");
    //if (OT)
      //Operand = OT->PatchTransformExpression(Operand);
    SS << "CurDAG->getTargetGlobalAddress(" << Operand << ", MVT::i32);"
       << endl;    
    SS << "  CurDAG->getMachineFunction().getInfo<`'__arch__`'Function"
          "Info>()->insertNewGlobalValue(cast<GlobalAddressSDNode>(" 
       << N
       << ")->getGlobal());" << endl;
    return SS.str();
  }
  
  //string GetBasicBlock(const string &N) {
  //}
  
  string MatchCondCode(const string &N, const string &S) {
    stringstream SS;
    SS << "cast<CondCodeSDNode>(" << N << ")->get() == ISD::" << S;
    return SS.str();
  }
  
  string MatchShortImm(const string &N, const string &S) {    
    stringstream SS;
    unsigned size = 16;
    unsigned mask = 0;
    for (unsigned i = 0; i < size; i++) {
      mask = (mask << 1) | 1;
    }
    mask = ~mask;
    SS << "(((unsigned) cast<ConstantSDNode>(" 
	      << N
	      << ")->getZExtValue()) & " << mask << "U) == 0";
    return SS.str();
  }
  
  string MatchTgtImm(const string &N, const string &S) {    
    stringstream SS;
    unsigned size = OperandTable.getType("tgtimm").Size;
    unsigned mask = 0;
    for (unsigned i = 0; i < size; i++) {
      mask = (mask << 1) | 1;
    }
    mask = ~mask;
    SS << "(((unsigned) cast<ConstantSDNode>(" 
	      << N
	      << ")->getZExtValue()) & " << mask << "U) == 0";
    return SS.str();
  }
  
  const unsigned NodeNamesSz = 20;
  
  const string NodeNames[] = { "load", 
			       "store",
			       "add",
			       "sub",
			       "shl",
			       "srl",
			       "or",
			       "call",
			       "ret",
			       "frameindex",
			       "imm",
			       "shortimm",
			       "tgtimm",
			       "texternalsymbol",
			       "tglobaladdr",
			       "globaladdr",
			       "br",
			       "br_cc",
			       "condcode",
			       "basicblock"
  };
  
  const string EnumNames[] = { "ISD::LOAD", 
			       "ISD::STORE",
			       "ISD::ADD",
			       "ISD::SUB",
			       "ISD::SHL",
			       "ISD::SRL",
			       "ISD::OR",
			       "SPARC16ISD::CALL", // BUG: Hardwired!
			       "SPARC16ISD::RETFLAG", // BUG: Hardwired!
			       "ISD::FrameIndex",
			       "ISD::Constant",
			       "ISD::Constant",
			       "ISD::Constant",
			       "ISD::TargetExternalSymbol",
			       "ISD::TargetGlobalAddress",
			       "ISD::GlobalAddress",
			       "ISD::BR",
			       "ISD::BR_CC",
			       "ISD::CONDCODE",
			       "ISD::BasicBlock"
  };
  
  GetNodeFunc FuncNames[] = { NULL, // load
			      NULL, // store
			      NULL, // add
			      NULL, // sub
			      NULL, // shl
			      NULL, // srl
			      NULL, // or
			      NULL, // call
			      NULL, // ret
			      GetFrameIndex, // frameindex
			      GetConstant, // imm
			      GetConstant, // shortimm
			      GetConstant, // tgtimm
			      NULL, // texternalsymbol
			      NULL, // tglobaladdr
			      GetGlobalAddress, // globaladdr
			      NULL, // br
			      NULL, // br_cc
			      NULL, // condcode
			      NULL  // basicblock
  };
  
  MatchNodeFunc MFuncNames[] = {  NULL, // load
				  NULL, // store
				  NULL, // add
				  NULL, // sub
				  NULL, // shl
				  NULL, // srl
				  NULL, // or
				  NULL, // call
				  NULL, // ret
				  NULL, // frameindex
				  NULL, // imm
				  MatchShortImm, //shortimm
				  MatchTgtImm, // tgtimm
				  NULL, // texternalsymbol
				  NULL, // tglobaladdr
				  NULL, // globaladdr
				  NULL, // br
				  NULL, // br_cc
				  MatchCondCode, // condcode
				  NULL  // basicblock
  };

  bool MatchChildrenVals[] = { true,  // load
			       true,  // store
			       true,  // add
			       true,  // sub
			       true,  // shl
			       true,  // srl
			       true,  // or
			       true,  // call
			       true,  // ret
			       false, // frameindex
			       false, // imm
			       false, // shortimm
			       false, // tgtimm
			       false, // texternalsymbol
			       false, // tglobaladdr
			       false, // globaladdr
			       true,  // br
			       true,  // br_cc
			       false, // condcode
			       false  // basicblock
  };
  
  bool HasChainVals[] = { true,  // load
			  true,  // store
			  false, // add
			  false, // sub
			  false, // shl
			  false, // srl
			  false, // or
			  true,  // call
			  true,  // ret
			  false, // frameindex
			  false, // imm
			  false, // shortimm
			  false, // tgtimm
			  false, // texternalsymbol
			  false, // tglobaladdr
			  false, // globaladdr
			  true,  // br
			  true,  // br_cc
			  false, // condcode
			  false  // basicblock
  };
  
  bool HasInFlagVals[] = {  false, // load
			    false, // store
			    false, // add
			    false, // sub
			    false, // shl
			    false, // srl
			    false, // or
			    false, // call
			    true,  // ret
			    false, // frameindex
			    false, // imm
			    false, // shortimm
			    false, // tgtimm
			    false, // texternalsymbol
			    false, // tglobaladdr
			    false, // globaladdr
			    false, // br
			    false, // br_cc
			    false, // condcode
			    false  // basicblock
  };
  
  bool HasOutFlagVals[] = { false, // load
			    false, // store
			    false, // add
			    false, // sub
			    false, // shl
			    false, // srl
			    false, // or
			    true,  // call
			    false, // ret
			    false, // frameindex
			    false, // imm
			    false, // shortimm
			    false, // tgtimm
			    false, // texternalsymbol
			    false, // tglobaladdr
			    false, // globaladdr
			    false, // br
			    false, // br_cc
			    false, // condcode
			    false  // basicblock
  };
  
  //const string * NodeToEnumName[] = { NodeNames, EnumNames };
  
}; // end anonymous namespace

LLVMNodeInfoMan* LLVMNodeInfoMan::ref = 0;

/// Singleton reference accessor method
LLVMNodeInfoMan* LLVMNodeInfoMan::getReference() {
  if (LLVMNodeInfoMan::ref != 0)
    return LLVMNodeInfoMan::ref;
  LLVMNodeInfoMan::ref = new LLVMNodeInfoMan();
  return LLVMNodeInfoMan::ref;
}

void LLVMNodeInfoMan::dispose () {
  if (LLVMNodeInfoMan::ref != 0)
    delete LLVMNodeInfoMan::ref;   
  LLVMNodeInfoMan::ref = 0;
}

///Private constructor
LLVMNodeInfoMan::LLVMNodeInfoMan() {
  Nodes.reserve(NodeNamesSz);
  
  for (unsigned i = 0; i < NodeNamesSz; ++i) {
    Nodes.push_back(LLVMNodeInfo(EnumNames[i], NodeNames[i], 
				 MatchChildrenVals[i], HasChainVals[i],
				 HasInFlagVals[i], HasOutFlagVals[i],
				 FuncNames[i], MFuncNames[i]));
    Map[NodeNames[i]] = &Nodes[i];
  }
}

///Private destructor
LLVMNodeInfoMan::~LLVMNodeInfoMan() {
  //delete [] Nodes;
}

/// Access information about node "Name". If it does not exist, throws
/// NameNotFoundException
const LLVMNodeInfo * LLVMNodeInfoMan::getInfo (const string & Name) const {
  if (Map.find(Name) == Map.end()) {
    std::cerr << "\"" << Name << "\"" << std::endl;
    throw NameNotFoundException();
  }
  return Map.find(Name)->second;
}
