//===-- __arch__`ISelDAGToDAG.cpp' - A dag to dag inst selector for __arch__ ------===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the __arch__ target.
//
//===----------------------------------------------------------------------===//

`#include "'__arch__`ISelLowering.h"'
`#include "'__arch__`TargetMachine.h"'
#include "llvm/Intrinsics.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===--------------------------------------------------------------------===//
/// __arch__`'DAGToDAGISel - __arch__ specific code to select __arch__ machine
/// instructions for SelectionDAG operations.
///
namespace {
class __arch__`'DAGToDAGISel : public SelectionDAGISel {
  /// Subtarget - Keep a pointer to the __arch__ Subtarget around so that we can
  /// make the right decision when generating code for different targets.
  const __arch__`'Subtarget &Subtarget;
public:
  explicit __arch__`'DAGToDAGISel(`'__arch__`'TargetMachine &TM)
    : SelectionDAGISel(TM),
      Subtarget(TM.getSubtarget<`'__arch__`'Subtarget>()) {
  }

  SDNode *Select(SDValue Op);
  
  /// Find an adequate register class to classify a virtual register
  const TargetRegisterClass* findSuitableRegClass(MVT vt);

  /// SelectInlineAsmMemoryOperand - Implement addressing mode selection for
  /// inline asm expressions.
  virtual bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                            char ConstraintCode,
                                            std::vector<SDValue> &OutOps);
					    
  bool SelectADDRri(SDValue Op, SDValue N, SDValue &Base);
  
  __patterns_headers__

  /// InstructionSelect - This callback is invoked by
  /// SelectionDAGISel when it has created a SelectionDAG for us to codegen.
  virtual void InstructionSelect();

  virtual const char *getPassName() const {
    return "`'__arch__ DAG->DAG Pattern Instruction Selection";
  }

  // Include the pieces autogenerated from the target description.
`#include "'__arch__`GenDAGISel.inc"'
};
}  // end anonymous namespace

const TargetRegisterClass*  __arch__`'DAGToDAGISel::findSuitableRegClass(MVT vt) {
  const TargetRegisterClass* BestFit = 0;
  unsigned NumRegs = 0;
  const TargetRegisterInfo *RI = CurDAG->getTarget().getRegisterInfo();
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

__simple_patterns__

/// InstructionSelect - This callback is invoked by
/// SelectionDAGISel when it has created a SelectionDAG for us to codegen.
void __arch__`'DAGToDAGISel::InstructionSelect() {
  DEBUG(BB->dump());

  // Select target instructions for the DAG.
  SelectRoot(*CurDAG);
  CurDAG->RemoveDeadNodes();
}

`SDNode *'__arch__`'DAGToDAGISel::Select(SDValue Op) {
  SDNode *N = Op.getNode();
  
  // Fixed patterns - autogenerated implementations
  switch(N->getOpcode()) {
    __patterns_switch__
  }
  
  if (N->isMachineOpcode())
    return NULL;   // Already selected.

  return SelectCode(Op);
}

// Select and extract frameindex constants
//bool __arch__`'DAGToDAGISel::SelectCPFI(SDValue Op, SDValue Addr, SDValue &Base) {
//  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
//    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);    
//    return true;
//  }
//  return false;
//}

/// SelectInlineAsmMemoryOperand - Implement addressing mode selection for
/// inline asm expressions.
bool
__arch__`'DAGToDAGISel::SelectInlineAsmMemoryOperand(const SDValue &Op,
                                                char ConstraintCode,
                                                std::vector<SDValue> &OutOps) {
  return true;
}

/// create`'__arch__`'ISelDag - This pass converts a legalized DAG into a
/// __arch__`'-specific DAG, ready for instruction scheduling.
///
FunctionPass *llvm::create`'__arch__`'ISelDag(`'__arch__`'TargetMachine &TM) {
  return new __arch__`'DAGToDAGISel(TM);
}
