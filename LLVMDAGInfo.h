//===- LLVMDAGInfo.h - --------------------------------------*- C++ -*-----===//
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

#ifndef LLVMDAGINFO_H
#define LLVMDAGINFO_H

#include "InsnSelector/TransformationRules.h"
#include <string>
#include <map>
#include <vector>

namespace LLVMDAGInfo {
  
  using std::string;
  using std::map;    
  using std::vector;
  using std::list;
  using backendgen::OperandTransformation;
  
  typedef string (*GetNodeFunc)(const string&, list<const OperandTransformation*>*);
  typedef string (*MatchNodeFunc)(const string&, const string&);
  
  struct LLVMNodeInfo {
    bool HasChain;
    bool HasInFlag;
    bool HasOutFlag;
    bool MatchChildren;
    string EnumName;
    string NodeName;
    GetNodeFunc GetNode;
    MatchNodeFunc MatchNode;
    
    LLVMNodeInfo(const string& E, const string& N, bool M,
		 bool C, bool InF, bool OutF,
		 GetNodeFunc GetF, MatchNodeFunc MatchF):
				    HasChain(C),
				    HasInFlag(InF),
				    HasOutFlag(OutF),
				    MatchChildren(M),
				    EnumName(E),
				    NodeName(N),
				    GetNode(GetF),
				    MatchNode(MatchF){}
  };
  
  struct NameNotFoundException {};
  
  class LLVMNodeInfoMan {
    static LLVMNodeInfoMan* ref;
    // Private constructor and destructor
    LLVMNodeInfoMan();
    ~LLVMNodeInfoMan();
    
    vector<LLVMNodeInfo> Nodes;
    map<string, const LLVMNodeInfo*> Map;
    
  public:
    static LLVMNodeInfoMan* getReference();
    static void dispose();
    const LLVMNodeInfo * getInfo(const string &Name) const;
  };
  
};

#endif

