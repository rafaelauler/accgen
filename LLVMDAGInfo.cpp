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
    string Transformations("GVGOESHERE");
    //assert (OTL == NULL && "GlobalAddress should not be OperandTransform'ed");
    if (OTL)
      HandleOTL(Transformations, OTL);
    SS << "CurDAG->getTargetGlobalAddress(" << Operand << ", MVT::i32,"
       << "(int64_t)  CurDAG->getMachineFunction().getInfo<`'__arch__`'Function"
          "Info>()->insertNewOperandExpression(\"" 
       << Transformations << "\"));" << endl;    
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
  
  // IntSize may be 1, 8 or 16
  template<int IntSize>
  string MatchUnindexedStore(const string &N, const string &S) {
    stringstream SS;
    SS << "cast<StoreSDNode>(" << N << ")->getAddressingMode() == ISD::UNINDEXED";
    if (IntSize > 0) {
      SS << " && cast<StoreSDNode>(" << N << ")->isTruncatingStore() && "
	    "cast<StoreSDNode>(" << N << ")->getMemoryVT() == MVT::i" << IntSize;
    } else {
       SS << " && !(cast<StoreSDNode>(" << N << ")->isTruncatingStore())";
    }
    return SS.str();
  }
  
  // IntSize may be 1, 8 or 16
  template<int IntSize>
  string MatchUnindexedSextLoad(const string &N, const string &S) {
    stringstream SS;
    SS << "cast<LoadSDNode>(" << N << ")->getAddressingMode() == ISD::UNINDEXED";
    if (IntSize > 0) {
      SS << " && cast<LoadSDNode>(" << N << ")->getExtensionType() == ISD::" 
         << "SEXTLOAD" <<
          " && cast<LoadSDNode>(" << N << ")->getMemoryVT() == MVT::i"
	 << IntSize;
    } else {
      SS << " && cast<LoadSDNode>(" << N << ")->getExtensionType() == ISD::" 
         << "NON_EXTLOAD";
    }
    return SS.str();
  }
  
  // IntSize may be 1, 8 or 16
  template<int IntSize>
  string MatchUnindexedZextLoad(const string &N, const string &S) {
    stringstream SS;
    SS << "cast<LoadSDNode>(" << N << ")->getAddressingMode() == ISD::UNINDEXED";
    if (IntSize > 0) {
      SS << " && (cast<LoadSDNode>(" << N << ")->getExtensionType() == ISD::" 
         << "ZEXTLOAD || cast<LoadSDNode>(" << N << ")->getExtensionType() == "
            "ISD::EXTLOAD)" <<
          " && cast<LoadSDNode>(" << N << ")->getMemoryVT() == MVT::i"
	 << IntSize;
    } else {
      SS << " && cast<LoadSDNode>(" << N << ")->getExtensionType() == ISD::" 
         << "NON_EXTLOAD";
    }
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
  
  const unsigned NodeNamesSz = 36;
  
  const string NodeNames[] = { "load", 
			       "loadsexti1",
			       "loadsexti8",
			       "loadsexti16",
			       "loadzexti1",
			       "loadzexti8",
			       "loadzexti16",
			       "store",
			       "truncstorei1",
			       "truncstorei8",
			       "truncstorei16",
			       "add",
			       "sub",
			       "mul",
			       "shl",
			       "srl",
			       "sra",
			       "rotl",
			       "rotr",
			       "or",
			       "and",
			       "xor",
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
			       "basicblock",
			       "sign_extend_inreg"
  };
  
  const string EnumNames[] = { "ISD::LOAD", 
			       "ISD::LOAD", 
			       "ISD::LOAD", 
			       "ISD::LOAD", 
			       "ISD::LOAD", 
			       "ISD::LOAD", 
			       "ISD::LOAD", 
			       "ISD::STORE",
			       "ISD::STORE",
			       "ISD::STORE",
			       "ISD::STORE",
			       "ISD::ADD",
			       "ISD::SUB",
			       "ISD::MUL",
			       "ISD::SHL",
			       "ISD::SRL",
			       "ISD::SRA",
			       "ISD::ROTL",
			       "ISD::ROTR",
			       "ISD::OR",
			       "ISD::AND",
			       "ISD::XOR",
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
			       "ISD::BasicBlock",
			       "ISD::SIGN_EXTEND_INREG"
  };
  
  GetNodeFunc FuncNames[] = { NULL, // load
			      NULL, // loadsexti1
			      NULL, // loadsexti8
			      NULL, // loadsexti16
			      NULL, // loadzexti1
			      NULL, // loadzexti8
			      NULL, // loadzexti16
			      NULL, // store
			      NULL, // truncstorei1
			      NULL, // truncstorei8
			      NULL, // truncstorei16
			      NULL, // add
			      NULL, // sub
			      NULL, // mul
			      NULL, // shl
			      NULL, // srl
			      NULL, // sra
			      NULL, // rotl
			      NULL, // rotr
			      NULL, // or
			      NULL, // and
			      NULL, // xor
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
			      NULL, // basicblock
			      NULL  // sign_extend_inreg
  };
  
  MatchNodeFunc MFuncNames[] = {  MatchUnindexedSextLoad<0>, // load
				  MatchUnindexedSextLoad<1>, // loadsexti1
				  MatchUnindexedSextLoad<8>, // loadsexti8
				  MatchUnindexedSextLoad<16>, // loadsexti16
				  MatchUnindexedZextLoad<1>, // loadzexti1
				  MatchUnindexedZextLoad<8>, // loadzexti8
				  MatchUnindexedZextLoad<16>, // loadzexti16
				  MatchUnindexedStore<0>, // store
				  MatchUnindexedStore<1>, // truncstorei1
				  MatchUnindexedStore<8>, // truncstorei8
				  MatchUnindexedStore<16>, // truncstorei16
				  NULL, // add
				  NULL, // sub
				  NULL, // mul
				  NULL, // shl
				  NULL, // srl
				  NULL, // sra
				  NULL, // rotl
				  NULL, // rotr
				  NULL, // or
				  NULL, // and
				  NULL, // xor
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
				  NULL,  // basicblock
				  NULL  // sign_extend_inreg
  };

  bool MatchChildrenVals[] = { true,  // load
			       true,  // loadsexti1
			       true,  // loadsexti8
			       true,  // loadsexti16
			       true,  // loadzexti1
			       true,  // loadzexti8
			       true,  // loadzexti16
			       true,  // store
			       true,  // truncstorei1
			       true,  // truncstorei8
			       true,  // truncstorei16
			       true,  // add
			       true,  // sub
			       true,  // mul
			       true,  // shl
			       true,  // srl
			       true,  // sra
			       true,  // rotl
			       true,  // rotr
			       true,  // or
			       true,  // and
			       true,  // xor
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
			       false, // basicblock
			       true   // sign_extend_inreg
  };
  
  bool HasChainVals[] = { true,  // load
  			  true,  // loadsexti1
			  true,  // loadsexti8
			  true,  // loadsexti16
			  true,  // loadzexti1
			  true,  // loadzexti8
			  true,  // loadzexti16
			  true,  // store
			  true,  // truncstorei1
			  true,  // truncstorei8
			  true,  // truncstorei16
			  false, // add
			  false, // sub
			  false, // mul
			  false, // shl
			  false, // srl
			  false, // sra
			  false, // rotl
			  false, // rotr
			  false, // or
			  false, // and
			  false, // xor
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
			  false, // basicblock
			  false  // sign_extend_inreg
  };
  
  bool HasInFlagVals[] = {  false, // load
			    false, // loadsexti1
			    false, // loadsexti8
			    false, // loadsexti16
			    false, // loadzexti1
			    false, // loadzexti8
			    false, // loadzexti16
			    false, // store
			    false, // truncstorei1
			    false, // truncstorei8
			    false, // truncstorei16
			    false, // add
			    false, // sub
			    false, // mul
			    false, // shl
			    false, // srl
			    false, // sra
			    false, // rotl
			    false, // rotr
			    false, // or
			    false, // and
			    false, // xor
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
			    false, // basicblock
			    false  // sign_extend_inreg
  };
  
  bool HasOutFlagVals[] = { false, // load
			    false, // loadsexti1
			    false, // loadsexti8
			    false, // loadsexti16
			    false, // loadzexti1
			    false, // loadzexti8
			    false, // loadzexti16
			    false, // store
			    false, // truncstorei1
			    false, // truncstorei8
			    false, // truncstorei16
			    false, // add
			    false, // sub
			    false, // mul
			    false, // shl
			    false, // srl
			    false, // sra
			    false, // rotl
			    false, // rotr
			    false, // or
			    false, // and
			    false, // xor
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
			    false, // basicblock
			    false  // sign_extend_inreg
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
