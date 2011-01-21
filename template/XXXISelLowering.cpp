//===-- __arch__`ISelLowering.cpp' - __arch__ DAG Lowering Implementation -===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file implements the interfaces that __arch__ uses to lower LLVM code 
// into a selection DAG.
//
//===----------------------------------------------------------------------===//

`#include "'__arch__`ISelLowering.h"'
`#include "'__arch__`TargetMachine.h"'
#include "llvm/Function.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/ADT/VectorExtras.h"
using namespace llvm;


//===----------------------------------------------------------------------===//
// Calling Convention Implementation
//===----------------------------------------------------------------------===//

`#include "'__arch__`GenCallingConv.inc"'

// AddLiveIn - This helper function adds the specified physical register to the
// MachineFunction as a live in value.  It also creates a corresponding
// virtual register for it.
static unsigned
AddLiveIn(MachineFunction &MF, unsigned PReg, const TargetRegisterClass *RC) 
{
  assert(RC->contains(PReg) && "Not the correct regclass!");
  unsigned VReg = MF.getRegInfo().createVirtualRegister(RC);
  MF.getRegInfo().addLiveIn(PReg, VReg);
  return VReg;
}

static SDValue LowerRET(SDValue Op, SelectionDAG &DAG) {
  // CCValAssign - represent the assignment of the return value to locations.
  SmallVector<CCValAssign, 16> RVLocs;
  unsigned CC = DAG.getMachineFunction().getFunction()->getCallingConv();
  bool isVarArg = DAG.getMachineFunction().getFunction()->isVarArg();

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CC, isVarArg, DAG.getTarget(), RVLocs);

  // Analize return values of ISD::RET
  CCInfo.AnalyzeReturn(Op.getNode(), RetCC_`'__arch__`');

  // If this is the first return lowered for this function, add the regs to the
  // liveout set for the function.
  if (DAG.getMachineFunction().getRegInfo().liveout_empty()) {
    for (unsigned i = 0; i != RVLocs.size(); ++i)
      if (RVLocs[i].isRegLoc())
        DAG.getMachineFunction().getRegInfo().addLiveOut(RVLocs[i].getLocReg());
  }

  SDValue Chain = Op.getOperand(0);
  SDValue Flag;

  // Copy the result values into the output registers.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    // ISD::RET => ret chain, (regnum1,val1), ...
    // So i*2+1 index only the regnums.
    Chain = DAG.getCopyToReg(Chain, VA.getLocReg(), Op.getOperand(i*2+1), Flag);

    // Guarantee that all emitted copies are stuck together with flags.
    Flag = Chain.getValue(1);
  }
  //unsigned RAreg = `'__ra_register__`';
  //unsigned RAreg = AddLiveIn(DAG.getMachineFunction(),`'__ra_register__`',
	//    DAG.getTarget().getRegisterInfo()->getPhysicalRegisterRegClass
	  //  (`'__ra_register__`', `MVT::i'__wordsize__`'));
  
__return_lowering__

  //if (Flag.getNode())
  //  return DAG.getNode(`'__arch_in_caps__`'ISD::RET_FLAG, MVT::Other, Chain, Flag);
  //return DAG.getNode(`'__arch_in_caps__`'ISD::RET_FLAG, MVT::Other, Chain);
}

//===----------------------------------------------------------------------===//
//                  CALL Calling Convention Implementation
//===----------------------------------------------------------------------===//

/// LowerCALL - functions arguments are copied from virtual regs to 
/// (physical regs)/(stack frame), CALLSEQ_START and CALLSEQ_END are emitted.
/// TODO: isVarArg, isTailCall.
SDValue __arch__`'TargetLowering::
LowerCALL(SDValue Op, SelectionDAG &DAG)
{
  MachineFunction &MF = DAG.getMachineFunction();

  CallSDNode *TheCall = cast<CallSDNode>(Op.getNode());
  SDValue Chain = TheCall->getChain();
  SDValue Callee = TheCall->getCallee();
  bool isVarArg = TheCall->isVarArg();
  unsigned CC = TheCall->getCallingConv();

  MachineFrameInfo *MFI = MF.getFrameInfo();

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CC, isVarArg, getTargetMachine(), ArgLocs);

  CCInfo.AnalyzeCallOperands(TheCall, CC_`'__arch__`');
  
  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getNextStackOffset();
  Chain = DAG.getCALLSEQ_START(Chain, DAG.getIntPtrConstant(NumBytes, true));

  // Use an arbitrary number to initialize vector (16)
  SmallVector<std::pair<unsigned, SDValue>, 16> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  // First/LastArgStackLoc contains the first/last 
  // "at stack" argument location.
  int LastArgStackLoc = 0;
  unsigned FirstStackArgLoc = 0;

  // Walk the register/memloc assignments, inserting copies/stores.
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    // Arguments start after the 5 first operands of ISD::CALL
    SDValue Arg = TheCall->getArg(i);
    
    // Promote the value if needed.
    switch (VA.getLocInfo()) {
    default: assert(0 && "Unknown loc info!");
    case CCValAssign::Full: break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, VA.getLocVT(), Arg);
      break;
    }
    
    // Arguments that can be passed on register must be kept at 
    // RegsToPass vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }
    
    // Register cant get to this point...
    assert(VA.isMemLoc());
    
    // Create the frame index object for this incoming parameter
    LastArgStackLoc = (FirstStackArgLoc + VA.getLocMemOffset());
    int FI = MFI->CreateFixedObject(VA.getValVT().getSizeInBits()/8,
                                    LastArgStackLoc);

    SDValue PtrOff = DAG.getFrameIndex(FI,getPointerTy());

    // emit ISD::STORE whichs stores the 
    // parameter value to a stack Location
    MemOpChains.push_back(DAG.getStore(Chain, Arg, PtrOff, NULL, 0));
  }

  // Transform all store nodes into one single node because all store
  // nodes are independent of each other.
  if (!MemOpChains.empty())     
    Chain = DAG.getNode(ISD::TokenFactor, MVT::Other, 
                        &MemOpChains[0], MemOpChains.size());

  // Build a sequence of copy-to-reg nodes chained together with token 
  // chain and flag operands which copy the outgoing args into registers.
  // The InFlag in necessary since all emited instructions must be
  // stuck together.
  SDValue InFlag;
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i) {
    Chain = DAG.getCopyToReg(Chain, RegsToPass[i].first, 
                             RegsToPass[i].second, InFlag);
    InFlag = Chain.getValue(1);
  }

  // If the callee is a GlobalAddress/ExternalSymbol node (quite common, every
  // direct call is) turn it into a TargetGlobalAddress/TargetExternalSymbol 
  // node so that legalize doesn't hack it. 
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) 
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), getPointerTy());
  else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), getPointerTy());


  // MipsJmpLink = #chain, #target_address, #opt_in_flags...
  //             = Chain, Callee, Reg#1, Reg#2, ...  
  //
  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Flag);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are 
  // known live into the call.
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i)
    Ops.push_back(DAG.getRegister(RegsToPass[i].first,
                                  RegsToPass[i].second.getValueType()));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain  = DAG.getNode(`'__arch_in_caps__`'ISD::CALL, NodeTys, &Ops[0], Ops.size());
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain, DAG.getIntPtrConstant(NumBytes, true),
                             DAG.getIntPtrConstant(0, true), InFlag);
  InFlag = Chain.getValue(1);

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return SDValue(LowerCallResult(Chain, InFlag, TheCall, CC, DAG), Op.getResNo());
}

/// LowerCallResult - Lower the result values of an ISD::CALL into the
/// appropriate copies out of appropriate physical registers.  This assumes that
/// Chain/InFlag are the input chain/flag to use, and that TheCall is the call
/// being lowered. Returns a SDNode with the same number of values as the 
/// ISD::CALL.
SDNode *`'__arch__`'TargetLowering::
LowerCallResult(SDValue Chain, SDValue InFlag, CallSDNode *TheCall, 
        unsigned CallingConv, SelectionDAG &DAG) {
  
  bool isVarArg = TheCall->isVarArg();

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallingConv, isVarArg, getTargetMachine(), RVLocs);

  CCInfo.AnalyzeCallResult(TheCall, RetCC_`'__arch__`');
  SmallVector<SDValue, 8> ResultVals;

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    Chain = DAG.getCopyFromReg(Chain, RVLocs[i].getLocReg(),
                                 RVLocs[i].getValVT(), InFlag).getValue(1);
    InFlag = Chain.getValue(2);
    ResultVals.push_back(Chain.getValue(0));
  }
  
  ResultVals.push_back(Chain);

  // Merge everything together with a MERGE_VALUES node.
  return DAG.getNode(ISD::MERGE_VALUES, TheCall->getVTList(),
                     &ResultVals[0], ResultVals.size()).getNode();
}

namespace {
const TargetRegisterClass* findSuitableRegClass(MVT vt, const TargetMachine &TM) {
  const TargetRegisterClass* BestFit = 0;
  unsigned NumRegs = 0;
  const TargetRegisterInfo *RI = TM.getRegisterInfo();
  for (TargetRegisterInfo::regclass_iterator I = RI->regclass_begin(),
    E = RI->regclass_end(); I != E; ++I) {
    if ((*I)->hasType(vt) &&
        (!BestFit || (*I)->getNumRegs() > NumRegs)) {      
      BestFit = *I;
      NumRegs = (*I)->getNumRegs();
    }
  }
  return BestFit;
}
}

/// LowerFORMAL_ARGUMENTS - transform physical registers into
/// virtual registers and generate load operations for
/// arguments places on the stack.
SDValue __arch__`'TargetLowering::
LowerFORMAL_ARGUMENTS(SDValue Op, SelectionDAG &DAG) 
{
  SDValue Root = Op.getOperand(0);
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  //MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();

  bool isVarArg = cast<ConstantSDNode>(Op.getOperand(2))->getZExtValue() != 0;
  unsigned CC = DAG.getMachineFunction().getFunction()->getCallingConv();

  unsigned StackReg = MF.getTarget().getRegisterInfo()->getFrameRegister(MF);

  // GP must be live into PIC and non-PIC call target.
  //AddLiveIn(MF, Mips::GP, Mips::CPURegsRegisterClass);

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CC, isVarArg, getTargetMachine(), ArgLocs);

  CCInfo.AnalyzeFormalArguments(Op.getNode(), CC_`'__arch__`');
  SmallVector<SDValue, 16> ArgValues;
  SDValue StackPtr;

  //unsigned FirstStackArgLoc = (Subtarget->isABI_EABI() ? 0 : 16);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {

    CCValAssign &VA = ArgLocs[i];

    // Arguments stored on registers
    if (VA.isRegLoc()) {
      MVT RegVT = VA.getLocVT();
      const TargetRegisterClass *RC = findSuitableRegClass(RegVT, getTargetMachine());
            
      assert(RC && "RegVT not supported by FORMAL_ARGUMENTS Lowering");

      // Transform the arguments stored on 
      // physical registers into virtual ones
      unsigned Reg = AddLiveIn(DAG.getMachineFunction(), VA.getLocReg(), RC);
      SDValue ArgValue = DAG.getCopyFromReg(Root, Reg, RegVT);
      
      // If this is an 8 or 16-bit value, it is really passed promoted 
      // to 32 bits.  Insert an assert[sz]ext to capture this, then 
      // truncate to the right size.
      if (VA.getLocInfo() == CCValAssign::SExt)
        ArgValue = DAG.getNode(ISD::AssertSext, RegVT, ArgValue,
                               DAG.getValueType(VA.getValVT()));
      else if (VA.getLocInfo() == CCValAssign::ZExt)
        ArgValue = DAG.getNode(ISD::AssertZext, RegVT, ArgValue,
                               DAG.getValueType(VA.getValVT()));
      
      if (VA.getLocInfo() != CCValAssign::Full)
        ArgValue = DAG.getNode(ISD::TRUNCATE, VA.getValVT(), ArgValue);

      ArgValues.push_back(ArgValue);

      // To meet ABI, when VARARGS are passed on registers, the registers
      // must have their values written to the caller stack frame. 
      if (isVarArg) {
        if (StackPtr.getNode() == 0)
          StackPtr = DAG.getRegister(StackReg, getPointerTy());
     
        // The stack pointer offset is relative to the caller stack frame. 
        // Since the real stack size is unknown here, a negative SPOffset 
        // is used so there's a way to adjust these offsets when the stack
        // size get known (on EliminateFrameIndex). A dummy SPOffset is 
        // used instead of a direct negative address (which is recorded to
        // be used on emitPrologue) to avoid mis-calc of the first stack 
        // offset on PEI::calculateFrameObjectOffsets.
        // Arguments are always 32-bit.
        int FI = MFI->CreateFixedObject(4, 0);
        //MipsFI->recordStoreVarArgsFI(FI, -(4+(i*4)));
        SDValue PtrOff = DAG.getFrameIndex(FI, getPointerTy());
      
        // emit ISD::STORE whichs stores the 
        // parameter value to a stack Location
        ArgValues.push_back(DAG.getStore(Root, ArgValue, PtrOff, NULL, 0));
      }

    } else { // VA.isRegLoc()

      // sanity check
      assert(VA.isMemLoc());
      
      // The stack pointer offset is relative to the caller stack frame. 
      // Since the real stack size is unknown here, a negative SPOffset 
      // is used so there's a way to adjust these offsets when the stack
      // size get known (on EliminateFrameIndex). A dummy SPOffset is 
      // used instead of a direct negative address (which is recorded to
      // be used on emitPrologue) to avoid mis-calc of the first stack 
      // offset on PEI::calculateFrameObjectOffsets.
      // Arguments are always 32-bit.
      unsigned ArgSize = VA.getLocVT().getSizeInBits()/8;
      int FI = MFI->CreateFixedObject(ArgSize, 5555); //magicnum to be detected by EliminateFrameIndex
      //MipsFI->recordLoadArgsFI(FI, -(ArgSize+
      //  (FirstStackArgLoc + VA.getLocMemOffset())));

      // Create load nodes to retrieve arguments from the stack
      SDValue FIN = DAG.getFrameIndex(FI, getPointerTy());
      ArgValues.push_back(DAG.getLoad(VA.getValVT(), Root, FIN, NULL, 0));
    }
  }

  // The mips ABIs for returning structs by value requires that we copy
  // the sret argument into $v0 for the return. Save the argument into
  // a virtual register so that we can access it from the return points.
  //if (DAG.getMachineFunction().getFunction()->hasStructRetAttr()) {
  //  unsigned Reg = MipsFI->getSRetReturnReg();
  //  if (!Reg) {
  //    Reg = MF.getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));
  //    MipsFI->setSRetReturnReg(Reg);
  //  }
  //  SDValue Copy = DAG.getCopyToReg(DAG.getEntryNode(), Reg, ArgValues[0]);
  //  Root = DAG.getNode(ISD::TokenFactor, MVT::Other, Copy, Root);
  //}

  ArgValues.push_back(Root);

  // Return the new list of results.
  return DAG.getNode(ISD::MERGE_VALUES, Op.getNode()->getVTList(),
                     &ArgValues[0], ArgValues.size()).getValue(Op.getResNo());
}


/// LowerGlobalAddress - just convert it to TargetConstant
SDValue __arch__`'TargetLowering::
LowerGLOBALADDRESS(SDValue Op, SelectionDAG &DAG)
{
  GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  return DAG.getTargetGlobalAddress(GV, `MVT::i'__wordsize__`');
}

/// LowerGlobalAddress - just convert it to TargetConstant
SDValue __arch__`'TargetLowering::
LowerCONSTANTPOOL(SDValue Op, SelectionDAG &DAG)
{
  ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);
  Constant *C = N->getConstVal();
  return DAG.getTargetConstantPool(C, `MVT::i'__wordsize__`', N->getAlignment());
}

namespace {
SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDValue TrueVal = Op.getOperand(2);
  SDValue FalseVal = Op.getOperand(3);
  unsigned Opc = 0;

  // If this is a select_cc of a "setcc", and if the setcc got lowered into
  // an CMP[IF]CC/SELECT_[IF]CC pair, find the original compared values.
  //LookThroughSetCC(LHS, RHS, CC, SPCC);

  SDValue CompareFlag;
  if (LHS.getValueType() == MVT::i32) {
    //std::vector<MVT> VTs;
    //VTs.push_back(LHS.getValueType());   // subcc returns a value
    //VTs.push_back(MVT::Flag);
    //SDValue Ops[2] = { LHS, RHS };
    //CompareFlag = DAG.getNode(SPISD::CMPICC, VTs, Ops, 2).getValue(1);
    Opc = `'__arch_in_caps__`'ISD::Select_CC;    
  } else {
    //CompareFlag = DAG.getNode(SPISD::CMPFCC, MVT::Flag, LHS, RHS);
    //Opc = SPISD::SELECT_FCC;
    //if (SPCC == ~0U) SPCC = FPCondCCodeToFCC(CC);
    assert (0 && "Cannot lower select_cc with float operands");
  }
  return DAG.getNode(Opc, TrueVal.getValueType(), TrueVal, FalseVal,
                     DAG.getConstant(CC, MVT::i32), LHS, RHS);
}
}

//===----------------------------------------------------------------------===//
// TargetLowering Implementation
//===----------------------------------------------------------------------===//


__arch__`'TargetLowering::`'__arch__`'TargetLowering(TargetMachine &TM)
  : TargetLowering(TM) {

  // Set up register classes
__set_up_register_classes__

  computeRegisterProperties();
    
  setOperationAction(ISD::MEMBARRIER, MVT::Other, Expand);
  setOperationAction(ISD::FSIN , MVT::f64, Expand);
  setOperationAction(ISD::FCOS , MVT::f64, Expand);
  setOperationAction(ISD::FREM , MVT::f64, Expand);
  setOperationAction(ISD::FSIN , MVT::f32, Expand);
  setOperationAction(ISD::FCOS , MVT::f32, Expand);
  setOperationAction(ISD::FREM , MVT::f32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ , MVT::i32, Expand);
  setOperationAction(ISD::CTLZ , MVT::i32, Expand);
  /*setOperationAction(ISD::ROTL , MVT::i32, Expand);
  setOperationAction(ISD::ROTR , MVT::i32, Expand);*/
  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::FCOPYSIGN, MVT::f64, Expand);
  setOperationAction(ISD::FCOPYSIGN, MVT::f32, Expand);
  setOperationAction(ISD::FPOW , MVT::f64, Expand);
  setOperationAction(ISD::FPOW , MVT::f32, Expand);
  
  setOperationAction(ISD::SDIV, MVT::i32, Expand);
  setOperationAction(ISD::UDIV, MVT::i32, Expand);
  setOperationAction(ISD::UREM, MVT::i32, Expand);
  setOperationAction(ISD::SREM, MVT::i32, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i32, Expand);
  
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
  
  // replace them with shl/sra
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8 , Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1 , Expand);
  
  // We have no select or setcc: expand to SELECT_CC.
  setOperationAction(ISD::SELECT, MVT::i32, Expand);
  setOperationAction(ISD::SELECT, MVT::f32, Expand);
  setOperationAction(ISD::SELECT, MVT::f64, Expand);
  setOperationAction(ISD::SETCC, MVT::i32, Expand);
  setOperationAction(ISD::SETCC, MVT::f32, Expand);
  setOperationAction(ISD::SETCC, MVT::f64, Expand);  

  setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);
  //setOperationAction(ISD::SELECT_CC, MVT::f32, Custom);
  //setOperationAction(ISD::SELECT_CC, MVT::f64, Custom);
  
  //setOperationAction(ISD::MUL, MVT::i32, Expand);
  
  //setOperationAction(ISD::GlobalAddress, `MVT::i'__wordsize__`', Custom);
  //setOperationAction(ISD::GlobalTLSAddress, `MVT::i'__wordsize__`', Custom);
  //setOperationAction(ISD::ConstantPool , `MVT::i'__wordsize__`', Custom);
  
  // RET must be custom lowered, to meet ABI requirements
  setOperationAction(ISD::RET               , MVT::Other, Custom);
}

`const char *'__arch__`'TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return 0;
  case __arch_in_caps__`'ISD::CMPICC:     return `"'__arch_in_caps__`'ISD::CMPICC";
  case __arch_in_caps__`'ISD::CMPFCC:     return `"'__arch_in_caps__`'ISD::CMPFCC";
  case __arch_in_caps__`'ISD::BRICC:      return `"'__arch_in_caps__`'ISD::BRICC";
  case __arch_in_caps__`'ISD::BRFCC:      return `"'__arch_in_caps__`'ISD::BRFCC";
  case __arch_in_caps__`'ISD::SELECT_ICC: return `"'__arch_in_caps__`'ISD::SELECT_ICC";
  case __arch_in_caps__`'ISD::SELECT_FCC: return `"'__arch_in_caps__`'ISD::SELECT_FCC";
  case __arch_in_caps__`'ISD::Hi:         return `"'__arch_in_caps__`'ISD::Hi";
  case __arch_in_caps__`'ISD::Lo:         return `"'__arch_in_caps__`'ISD::Lo";
  case __arch_in_caps__`'ISD::FTOI:       return `"'__arch_in_caps__`'ISD::FTOI";
  case __arch_in_caps__`'ISD::ITOF:       return `"'__arch_in_caps__`'ISD::ITOF";
  case __arch_in_caps__`'ISD::CALL:       return `"'__arch_in_caps__`'ISD::CALL";
  case __arch_in_caps__`'ISD::RET_FLAG:   return `"'__arch_in_caps__`'ISD::RET_FLAG";
  }
}

/// isMaskedValueZeroForTargetNode - Return true if 'Op & Mask' is known to
/// be zero. Op is expected to be a target specific node. Used by DAG
/// combiner.
void __arch__`'TargetLowering::computeMaskedBitsForTargetNode(const SDValue Op,
                                                        const APInt &Mask,
                                                        APInt &KnownZero,
                                                        APInt &KnownOne,
                                                        const SelectionDAG &DAG,
                                                        unsigned Depth) const {
  KnownZero = KnownOne = APInt(Mask.getBitWidth(), 0);   // Don't know anything.

}
#include <iostream>
SDValue __arch__`'TargetLowering::
LowerOperation(SDValue Op, SelectionDAG &DAG) {
  std::cout << "Operation name:" << Op.getNode()->getOperationName() << "\n";

  switch(Op.getOpcode()) {
  default: assert(0 && "Should not custom lower this!");
  case ISD::CALL:            
    return LowerCALL(Op, DAG);
  case ISD::GlobalTLSAddress:
    assert(0 && "TLS not implemented.");
  case ISD::GlobalAddress:      return LowerGLOBALADDRESS(Op, DAG);
  case ISD::ConstantPool:       return LowerCONSTANTPOOL(Op, DAG);  
  case ISD::RET:                return LowerRET(Op, DAG);
  case ISD::FORMAL_ARGUMENTS:   return LowerFORMAL_ARGUMENTS(Op, DAG);
  case ISD::SELECT_CC:          return LowerSELECT_CC(Op, DAG);
  }

}

MachineBasicBlock *
__arch__`'TargetLowering::EmitInstrWithCustomInserter(MachineInstr *MI,
                                                 MachineBasicBlock *BB) {
  const TargetInstrInfo &TII = *(getTargetMachine().getInstrInfo());

  switch (MI->getOpcode()) {
  default: //assert(false && "Unexpected instr type to insert");
  case `'__arch_in_caps__`'ISD::Select_CC: {
    // To "insert" a SELECT_CC instruction, we actually have to insert the
    // diamond control-flow pattern.  The incoming instruction knows the
    // destination vreg to set, the condition code register to branch on, the
    // true/false values to select between, and a branch opcode to use.
    const BasicBlock *LLVM_BB = BB->getBasicBlock();
    MachineFunction::iterator It = BB;
    ++It;

    //  thisMBB:
    //  ...
    //   TrueVal = ...
    //   setcc r1, r2, r3
    //   bNE   r1, r0, copy1MBB
    //   fallthrough --> copy0MBB
    MachineBasicBlock *thisMBB  = BB;
    MachineFunction *F = BB->getParent();
    MachineBasicBlock *copy0MBB = F->CreateMachineBasicBlock(LLVM_BB);
    MachineBasicBlock *sinkMBB  = F->CreateMachineBasicBlock(LLVM_BB);

    // Emit the right instruction according to the type of the operands compared
    //if (isFPCmp) {
      // Find the condiction code present in the setcc operation.
      //Mips::CondCode CC = (Mips::CondCode)MI->getOperand(4).getImm();
      // Get the branch opcode from the branch code.
      //unsigned Opc = FPBranchCodeToOpc(GetFPBranchCodeFromCond(CC));
      //BuildMI(BB, TII->get(Opc)).addMBB(sinkMBB);
    //} else
    unsigned CondCode = MI->getOperand(3).getImm();
    int TrueVal = MI->getOperand(1).getReg();
    int FalseVal = MI->getOperand(2).getReg();
    int lhs = MI->getOperand(4).getReg();
    int rhs = MI->getOperand(5).getReg();
    MachineBasicBlock *tgt = sinkMBB;
__emit_select_cc__
      //BuildMI(BB, TII->get(Mips::BNE)).addReg(MI->getOperand(1).getReg())
      //  .addReg(Mips::ZERO).addMBB(sinkMBB);

    F->insert(It, copy0MBB);
    F->insert(It, sinkMBB);
    // Update machine-CFG edges by first adding all successors of the current
    // block to the new block which will contain the Phi node for the select.
    for(MachineBasicBlock::succ_iterator i = BB->succ_begin(),
        e = BB->succ_end(); i != e; ++i)
      sinkMBB->addSuccessor(*i);
    // Next, remove all successors of the current block, and add the true
    // and fallthrough blocks as its successors.
    while(!BB->succ_empty())
      BB->removeSuccessor(BB->succ_begin());
    BB->addSuccessor(copy0MBB);
    BB->addSuccessor(sinkMBB);

    //  copy0MBB:
    //   %FalseValue = ...
    //   # fallthrough to sinkMBB
    BB = copy0MBB;

    // Update machine-CFG edges
    BB->addSuccessor(sinkMBB);

    //  sinkMBB:
    //   %Result = phi [ %FalseValue, copy0MBB ], [ %TrueValue, thisMBB ]
    //  ...
    BB = sinkMBB;
    BuildMI(BB, TII.get(`'__arch__`'::PHI), MI->getOperand(0).getReg())
      .addReg(FalseVal).addMBB(copy0MBB)
      .addReg(TrueVal).addMBB(thisMBB);

    F->DeleteMachineInstr(MI);   // The pseudo instruction is gone now.
    return BB;
  }
  }
}

//===----------------------------------------------------------------------===//
//                         __arch__ Inline Assembly Support
//===----------------------------------------------------------------------===//

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
__arch__`'TargetLowering::ConstraintType
__arch__`'TargetLowering::getConstraintType(const std::string &Constraint) const {
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass*>
__arch__`'TargetLowering::getRegForInlineAsmConstraint(const std::string &Constraint,
                                                  MVT VT) const {
  return TargetLowering::getRegForInlineAsmConstraint(Constraint, VT);
}

std::vector<unsigned> __arch__`'TargetLowering::
getRegClassForInlineAsmConstraint(const std::string &Constraint,
                                  MVT VT) const {
  if (Constraint.size() != 1)
    return std::vector<unsigned>();

  return std::vector<unsigned>();
}

bool
__arch__`'TargetLowering::isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
  return false;
}
