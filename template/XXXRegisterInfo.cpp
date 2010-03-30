// `'__arch__`'RegisterInfo.cpp - `'__arch__`' Register Information --*- C++ -*-//
//
//                 The ArchC LLVM Backend Generator
//
// Automatically generated file. 
//
//===----------------------------------------------------------------------===//
//
// This file contains the `'__arch__`' implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

`#define DEBUG_TYPE "'__arch__`-reg-info"'

`#include "'__arch__`.h"'
`#include "'__arch__`'Subtarget.h"
`#include "'__arch__`'RegisterInfo.h"
`#include "'__arch__`'MachineFunction.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
#include "llvm/Function.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineLocation.h"
#include "llvm/Target/TargetFrameInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/STLExtras.h"

using namespace llvm;

`'__arch__`'RegisterInfo::`'__arch__`'RegisterInfo(const `'__arch__`'Subtarget &ST, 
                                   const TargetInstrInfo &tii)
  : `'__arch__`'GenRegisterInfo(`'__arch__`'::ADJCALLSTACKDOWN, `'__arch__`'::ADJCALLSTACKUP),
    Subtarget(ST), TII(tii) {}

//===----------------------------------------------------------------------===//
// Callee Saved Registers methods 
//===----------------------------------------------------------------------===//

/// `'__arch__`' Callee Saved Registers
const unsigned* `'__arch__`'RegisterInfo::
getCalleeSavedRegs(const MachineFunction *MF) const 
{
  `'__callee_saved_reg_list__`'

  return CalleeSavedRegs;
}

/// `'__arch__`' Callee Saved Register Classes
const TargetRegisterClass* const* 
`'__arch__`'RegisterInfo::getCalleeSavedRegClasses(const MachineFunction *MF) const 
{
  `'__callee_saved_reg_classes_list__`'

  return CalleeSavedRC;
}

BitVector `'__arch__`'RegisterInfo::
getReservedRegs(const MachineFunction &MF) const
{
  BitVector Reserved(getNumRegs());
`'__reserved_list__`'
  return Reserved;
}

// hasFP - Return true if the specified function should have a dedicated frame
// pointer register.  This is true if the function has variable sized allocas or
// if frame pointer elimination is disabled.
bool __arch__`'RegisterInfo::
hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo *MFI = MF.getFrameInfo();
  return NoFramePointerElim || MFI->hasVarSizedObjects();
}

// This function eliminate ADJCALLSTACKDOWN, 
// ADJCALLSTACKUP pseudo instructions
void __arch__`'RegisterInfo::
eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator I) const {
  // Simply discard ADJCALLSTACKDOWN, ADJCALLSTACKUP instructions.
  MBB.erase(I);
}

// FrameIndex represent objects inside a abstract stack.
// We must replace FrameIndex with an stack/frame pointer
// direct reference.
void __arch__`'RegisterInfo::
eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj, 
                    RegScavenger *RS) const 
{
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();

  unsigned i = 0;
  while (!MI.getOperand(i).isFI()) {
    ++i;
    assert(i < MI.getNumOperands() && 
           "Instr doesn't have FrameIndex operand!");
  }

  #ifndef NDEBUG
  DOUT << "\nFunction : " << MF.getFunction()->getName() << "\n";
  DOUT << "<--------->\n";
  MI.print(DOUT);
  #endif

  int FrameIndex = MI.getOperand(i).getIndex();
  int stackSize  = MF.getFrameInfo()->getStackSize();
  int spOffset   = MF.getFrameInfo()->getObjectOffset(FrameIndex);

  #ifndef NDEBUG
  DOUT << "FrameIndex : " << FrameIndex << "\n";
  DOUT << "spOffset   : " << spOffset << "\n";
  DOUT << "stackSize  : " << stackSize << "\n";
  #endif

  // as explained on LowerFORMAL_ARGUMENTS, detect negative offsets 
  // and adjust SPOffsets considering the final stack size.
  int Offset = ((spOffset < 0) ? (stackSize + (-(spOffset+4))) : (spOffset));
  Offset    += MI.getOperand(i-1).getImm();

  #ifndef NDEBUG
  DOUT << "Offset     : " << Offset << "\n";
  DOUT << "<--------->\n";
  #endif

  MI.getOperand(i-1).ChangeToImmediate(Offset);
  MI.getOperand(i).ChangeToRegister(getFrameRegister(MF), false);
}

void __arch__`'RegisterInfo::
emitPrologue(MachineFunction &MF) const 
{
 
}

void __arch__`'RegisterInfo::
emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const 
{
 
}


void __arch__`'RegisterInfo::
processFunctionBeforeFrameFinalized(MachineFunction &MF) const {

}

unsigned __arch__`'RegisterInfo::
getRARegister() const {
  return 0;
}

unsigned __arch__`'RegisterInfo::
getFrameRegister(MachineFunction &MF) const {
  return 0;
}

unsigned __arch__`'RegisterInfo::
getEHExceptionRegister() const {
  assert(0 && "What is the exception register");
  return 0;
}

unsigned __arch__`'RegisterInfo::
getEHHandlerRegister() const {
  assert(0 && "What is the exception handler register");
  return 0;
}

int __arch__`'RegisterInfo::
getDwarfRegNum(unsigned RegNum, bool isEH) const {
  assert(0 && "What is the dwarf register number");
  return -1;
}

`#include "'__arch__`GenRegisterInfo.inc"'
