//===- __arch__`InstrInfo.cpp' - __arch Instruction Information -------*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file contains the __arch__ implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

`#include "'__arch__`InstrInfo.h"'
`#include "'__arch__`Subtarget.h"'
`#include "'__arch__`.h"'
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetMachine.h"
`#include "'__arch__`GenInstrInfo.inc"'
using namespace llvm;

__arch__`'InstrInfo::__arch__`'InstrInfo(__arch__`'Subtarget &ST)
  : TargetInstrInfoImpl(`'__arch__`'Insts, array_lengthof(`'__arch__`'Insts)),
    RI(ST, *this), Subtarget(ST) {
}

namespace {
const TargetRegisterClass* findSuitableRegClass(MVT vt, const MachineBasicBlock &MBB) {
  const TargetRegisterClass* BestFit = 0;
  unsigned NumRegs = 0;
  const TargetRegisterInfo *RI = MBB.getParent()->getTarget().getRegisterInfo();
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

MachineInstr* findNearestDefBefore(MachineInstr* MI, unsigned Reg) {
  //const MachineFunction* MF = MI->getParent()->getParent();  
  MachineBasicBlock *I = MI->getParent();
  MachineInstr* NearestDef = NULL;  
  bool found = false;
  //for (MachineFunction::iterator I = MF->begin(), E = MF->end();
  //     I != E && !found; ++ I) {
    for (MachineBasicBlock::iterator I2 = I->begin(), E2 = I->end();
	 I2 != E2; ++I2) {
      if (&(*I2) == MI) {
	found = true;
	break;
      }
      if (I2->findRegisterDefOperandIdx(Reg) != -1)
	NearestDef = &*I2;
    }
  //}
  return NearestDef;
}

MachineInstr* findNearestDefAfter(MachineInstr* MI, unsigned Reg) {
  //const MachineFunction* MF = MI->getParent()->getParent();  
  MachineBasicBlock *I = MI->getParent();
  MachineInstr* NearestDef = NULL;  
  bool found = false;
  //for (MachineFunction::iterator I = MF->begin(), E = MF->end();
  //     I != E && !found; ++ I) {
    for (MachineBasicBlock::iterator I2 = I->begin(), E2 = I->end();
	 I2 != E2; ++I2) {
      if (&(*I2) == MI) {
	found = true;
	continue;
      }
      if (I2->findRegisterDefOperandIdx(Reg) != -1 && found) {
	NearestDef = &*I2;
	break;
      }
    }
  //}
  return NearestDef;
}

bool isLive(MachineInstr* MI) {
  //const MachineFunction* MF = MI->getParent()->getParent();  
  MachineBasicBlock *I = MI->getParent();
  unsigned RegDef = 0;
  bool found = false;
  //for (MachineFunction::iterator I = MF->begin(), E = MF->end();
  //     I != E && !found; ++ I) {
    for (MachineBasicBlock::iterator I2 = I->begin(), E2 = I->end();
	 I2 != E2; ++I2) {
      if (&(*I2) == MI) {
	found = true;
	if (I2->getOperand(0).isReg())
	  RegDef = I2->getOperand(0).getReg();
	else 
	  return false;
	continue;
      }
      if (I2->findRegisterUseOperandIdx(RegDef) != -1 && found) 
	return true;      
      if (I2->findRegisterDefOperandIdx(RegDef) != -1 && found) 
	return false;      
    }
  //}
  return false;
}

}

//static bool isZeroImm(const MachineOperand &op) {
//  return op.isImm() && op.getImm() == 0;
//}

/// Return true if the instruction is a register to register move and
/// leave the source and dest operands in the passed parameters.
///
bool `'__arch__`'InstrInfo::isMoveInstr(const MachineInstr &MI,
                                 unsigned &SrcReg, unsigned &DstReg,
                                 unsigned &SrcSR, unsigned &DstSR) const {
  SrcSR = DstSR = 0; // No sub-registers.
  MachineInstr* pMI = const_cast<MachineInstr*>(&MI);
  __is_move__
}

/// isLoadFromStackSlot - If the specified machine instruction is a direct
/// load from a stack slot, return the virtual or physical register number of
/// the destination along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than loading from the stack slot.
unsigned `'__arch__`'InstrInfo::isLoadFromStackSlot(const MachineInstr *MI,
                                             int &FrameIndex) const {
  MachineInstr* pMI = const_cast<MachineInstr*>(MI);
#define getFrameIndex getIndex
#define isFrameIndex isFI
__is_load_from_stack_slot__
#undef getFrameIndex
#undef isFrameIndex
  return 0;
}

/// isStoreToStackSlot - If the specified machine instruction is a direct
/// store to a stack slot, return the virtual or physical register number of
/// the source reg along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than storing to the stack slot.
unsigned `'__arch__`'InstrInfo::isStoreToStackSlot(const MachineInstr *MI,
                                            int &FrameIndex) const {
  MachineInstr* pMI = const_cast<MachineInstr*>(MI);
#define getFrameIndex getIndex
#define isFrameIndex isFI	
__is_store_to_stack_slot__
#undef getFrameIndex
#undef isFrameIndex
  return 0;
}

unsigned
`'__arch__`'InstrInfo::InsertBranch(MachineBasicBlock &MBB,MachineBasicBlock *TBB,
                             MachineBasicBlock *FBB,
                             const SmallVectorImpl<MachineOperand> &Cond)const{
  assert (0 && "InsertBranch not implemented.");
  return 0;
}

bool `'__arch__`'InstrInfo::copyRegToReg(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     unsigned DestReg, unsigned SrcReg,
                                     const TargetRegisterClass *DestRC,
                                     const TargetRegisterClass *SrcRC) const {

  
__copyregpats__
  
  return true;
}

void `'__arch__`'InstrInfo::
storeRegToStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                    unsigned SrcReg, bool isKill, int FI,
                    const TargetRegisterClass *RC) const {

  unsigned src = SrcReg;
  int val = FI;
__store_reg_to_stack_slot__
}

void `'__arch__`'InstrInfo::storeRegToAddr(MachineFunction &MF, unsigned SrcReg,
                                       bool isKill,
                                       SmallVectorImpl<MachineOperand> &Addr,
                                       const TargetRegisterClass *RC,
                                 SmallVectorImpl<MachineInstr*> &NewMIs) const {
  assert (0 && "storeRegToAddr not implemented.");
}

void `'__arch__`'InstrInfo::
loadRegFromStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                     unsigned DestReg, int FI,
                     const TargetRegisterClass *RC) const {
  unsigned dest = DestReg;
  int addr = FI;
__load_reg_from_stack_slot__ 
}

void `'__arch__`'InstrInfo::loadRegFromAddr(MachineFunction &MF, unsigned DestReg,
                                        SmallVectorImpl<MachineOperand> &Addr,
                                        const TargetRegisterClass *RC,
                                 SmallVectorImpl<MachineInstr*> &NewMIs) const {
  assert (0 && "loadRegFromAddr not implemented.");
}

MachineInstr *`'__arch__`'InstrInfo::foldMemoryOperandImpl(MachineFunction &MF,
                                                    MachineInstr* MI,
                                          const SmallVectorImpl<unsigned> &Ops,
                                                    int FI) const {
  //assert (0 && "foldMemoryOperandImpl not implemented.");
  return NULL;
}
