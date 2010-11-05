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
    SS << "CurDAG->getTargetConstant(dyn_cast<FrameIndexSDNode>(" 
       << N
       << ")->getIndex(), MVT::i32);"
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
  
  const unsigned NodeNamesSz = 7;
  
  const string NodeNames[] = { "load", 
			       "store",
			       "add",
			       "call",
			       "frameindex",
			       "imm",
			       "tglobaladdr"
  };
  
  const string EnumNames[] = { "ISD::LOAD", 
			       "ISD::STORE",
			       "ISD::ADD",
			       "SPARC16ISD::CALL", // BUG: Hardwired!
			       "ISD::FrameIndex",
			       "ISD::Constant",
			       "ISD::TargetGlobalAddress"
  };
  
  GetNodeFunc FuncNames[] = { NULL,
			      NULL,
			      NULL,
			      NULL,
			      GetFrameIndex,
			      GetConstant,
			      NULL
  };

  bool MatchChildrenVals[] = { true,
			       true,
			       true,
			       true,
			       false,
			       false,
			       false
  };
  
  bool HasChainVals[] = { true,
			  true,
			  false,
			  true,
			  false,
			  false,
			  false
  };
  
  bool HasInFlagVals[] = {  false,
			    false,
			    false,
			    false,
			    false,
			    false,
			    false
  };
  
  bool HasOutFlagVals[] = { false,
			    false,
			    false,
			    true,
			    false,
			    false,
			    false
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
				 FuncNames[i]));
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
