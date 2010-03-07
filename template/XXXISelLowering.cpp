//===-- __arch__`ISelLowering.cpp' - __arch__ DAG Lowering Implementation ---------===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file implements the interfaces that __arch__ uses to lower LLVM code into a
// selection DAG.
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

void
__arch__`'TargetLowering::LowerArguments(Function &F, SelectionDAG &DAG,
                                    SmallVectorImpl<SDValue> &ArgValues,
                                    DebugLoc dl) {
 
}



//===----------------------------------------------------------------------===//
// TargetLowering Implementation
//===----------------------------------------------------------------------===//


__arch__`'TargetLowering::`'__arch__`'TargetLowering(TargetMachine &TM)
  : TargetLowering(TM) {

  computeRegisterProperties();
}

`const char *'__arch__`'TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return 0;
  case __arch_in_caps__`'ISD::CMPICC:     return "SPISD::CMPICC";
  case __arch_in_caps__`'ISD::CMPFCC:     return "SPISD::CMPFCC";
  case __arch_in_caps__`'ISD::BRICC:      return "SPISD::BRICC";
  case __arch_in_caps__`'ISD::BRFCC:      return "SPISD::BRFCC";
  case __arch_in_caps__`'ISD::SELECT_ICC: return "SPISD::SELECT_ICC";
  case __arch_in_caps__`'ISD::SELECT_FCC: return "SPISD::SELECT_FCC";
  case __arch_in_caps__`'ISD::Hi:         return "SPISD::Hi";
  case __arch_in_caps__`'ISD::Lo:         return "SPISD::Lo";
  case __arch_in_caps__`'ISD::FTOI:       return "SPISD::FTOI";
  case __arch_in_caps__`'ISD::ITOF:       return "SPISD::ITOF";
  case __arch_in_caps__`'ISD::CALL:       return "SPISD::CALL";
  case __arch_in_caps__`'ISD::RET_FLAG:   return "SPISD::RET_FLAG";
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

SDValue __arch__`'TargetLowering::
LowerOperation(SDValue Op, SelectionDAG &DAG) {
  assert(0 && "Should not custom lower this!");
}

MachineBasicBlock *
__arch__`'TargetLowering::EmitInstrWithCustomInserter(MachineInstr *MI,
                                                 MachineBasicBlock *BB) {
  return NULL;
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
