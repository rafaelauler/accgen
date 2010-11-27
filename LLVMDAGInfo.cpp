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
#include <iostream>
#include <sstream>

using namespace LLVMDAGInfo;
using std::stringstream;
using std::string;
using std::endl;
using std::map;

namespace {
  
  string GetFrameIndex(const string &N) {
    stringstream SS;
    SS << "CurDAG->getTargetFrameIndex(dyn_cast<FrameIndexSDNode>(" 
       << N
       << ")->getIndex(), TLI.getPointerTy());"
       << endl;    
    return SS.str();
  }
  
  string GetConstant(const string &N) {
    stringstream SS;    
    SS << "CurDAG->getTargetConstant(((unsigned) cast<ConstantSDNode>(" 
       << N
       << ")->getZExtValue()), MVT::i32);"
       << endl;    
    return SS.str();
  }
  
  string GetGlobalAddress(const string &N) {
    stringstream SS;        
    SS << "CurDAG->getTargetGlobalAddress(cast<GlobalAddressSDNode>(" 
       << N
       << ")->getGlobal(), MVT::i32);"
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
  
  const unsigned NodeNamesSz = 16;
  
  const string NodeNames[] = { "load", 
			       "store",
			       "add",
			       "sub",
			       "shl",
			       "call",
			       "ret",
			       "frameindex",
			       "imm",
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
			       "SPARC16ISD::CALL", // BUG: Hardwired!
			       "SPARC16ISD::RETFLAG", // BUG: Hardwired!
			       "ISD::FrameIndex",
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
			      NULL, // call
			      NULL, // ret
			      GetFrameIndex, // frameindex
			      GetConstant, // imm
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
				  NULL, // call
				  NULL, // ret
				  NULL, // frameindex
				  NULL, // imm
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
			       true,  // call
			       true,  // ret
			       false, // frameindex
			       false, // imm
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
			  true,  // call
			  true,  // ret
			  false, // frameindex
			  false, // imm
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
			    false, // call
			    true,  // ret
			    false, // frameindex
			    false, // imm
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
			    true,  // call
			    false, // ret
			    false, // frameindex
			    false, // imm
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
