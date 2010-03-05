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

#define DEBUG_TYPE "mips-reg-info"

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

void MipsRegisterInfo::
emitPrologue(MachineFunction &MF) const 
{
  MachineBasicBlock &MBB   = MF.front();
  MachineFrameInfo *MFI    = MF.getFrameInfo();
  MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  bool isPIC = (MF.getTarget().getRelocationModel() == Reloc::PIC_);

  // Get the right frame order for Mips.
  adjustMipsStackFrame(MF);

  // Get the number of bytes to allocate from the FrameInfo.
  unsigned StackSize = MFI->getStackSize();

  // No need to allocate space on the stack.
  if (StackSize == 0 && !MFI->hasCalls()) return;

  int FPOffset = MipsFI->getFPStackOffset();
  int RAOffset = MipsFI->getRAStackOffset();

  BuildMI(MBB, MBBI, TII.get(Mips::NOREORDER));
  
  // TODO: check need from GP here.
  if (isPIC && Subtarget.isABI_O32()) 
    BuildMI(MBB, MBBI, TII.get(Mips::CPLOAD)).addReg(getPICCallReg());
  BuildMI(MBB, MBBI, TII.get(Mips::NOMACRO));

  // Adjust stack : addi sp, sp, (-imm)
  BuildMI(MBB, MBBI, TII.get(Mips::ADDiu), Mips::SP)
      .addReg(Mips::SP).addImm(-StackSize);

  // Save the return address only if the function isnt a leaf one.
  // sw  $ra, stack_loc($sp)
  if (MFI->hasCalls()) { 
    BuildMI(MBB, MBBI, TII.get(Mips::SW))
        .addReg(Mips::RA).addImm(RAOffset).addReg(Mips::SP);
  }

  // if framepointer enabled, save it and set it
  // to point to the stack pointer
  if (hasFP(MF)) {
    // sw  $fp,stack_loc($sp)
    BuildMI(MBB, MBBI, TII.get(Mips::SW))
      .addReg(Mips::FP).addImm(FPOffset).addReg(Mips::SP);

    // move $fp, $sp
    BuildMI(MBB, MBBI, TII.get(Mips::ADDu), Mips::FP)
      .addReg(Mips::SP).addReg(Mips::ZERO);
  }

  // PIC speficic function prologue
  if ((isPIC) && (MFI->hasCalls())) {
    BuildMI(MBB, MBBI, TII.get(Mips::CPRESTORE))
      .addImm(MipsFI->getGPStackOffset());
  }
}

void MipsRegisterInfo::
emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const 
{
  MachineBasicBlock::iterator MBBI = prior(MBB.end());
  MachineFrameInfo *MFI            = MF.getFrameInfo();
  MipsFunctionInfo *MipsFI         = MF.getInfo<MipsFunctionInfo>();

  // Get the number of bytes from FrameInfo
  int NumBytes = (int) MFI->getStackSize();

  // Get the FI's where RA and FP are saved.
  int FPOffset = MipsFI->getFPStackOffset();
  int RAOffset = MipsFI->getRAStackOffset();

  // if framepointer enabled, restore it and restore the
  // stack pointer
  if (hasFP(MF)) {
    // move $sp, $fp
    BuildMI(MBB, MBBI, TII.get(Mips::ADDu), Mips::SP)
      .addReg(Mips::FP).addReg(Mips::ZERO);

    // lw  $fp,stack_loc($sp)
    BuildMI(MBB, MBBI, TII.get(Mips::LW))
      .addReg(Mips::FP).addImm(FPOffset).addReg(Mips::SP);
  }

  // Restore the return address only if the function isnt a leaf one.
  // lw  $ra, stack_loc($sp)
  if (MFI->hasCalls()) { 
    BuildMI(MBB, MBBI, TII.get(Mips::LW))
      .addReg(Mips::RA).addImm(RAOffset).addReg(Mips::SP);
  }

  // adjust stack  : insert addi sp, sp, (imm)
  if (NumBytes) {
    BuildMI(MBB, MBBI, TII.get(Mips::ADDiu), Mips::SP)
      .addReg(Mips::SP).addImm(NumBytes);
  }
}


void MipsRegisterInfo::
processFunctionBeforeFrameFinalized(MachineFunction &MF) const {
  // Set the SPOffset on the FI where GP must be saved/loaded.
  MachineFrameInfo *MFI = MF.getFrameInfo();
  bool isPIC = (MF.getTarget().getRelocationModel() == Reloc::PIC_);
  if (MFI->hasCalls() && isPIC) { 
    MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();
    MFI->setObjectOffset(MipsFI->getGPFI(), MipsFI->getGPStackOffset());
  }    
}

unsigned MipsRegisterInfo::
getRARegister() const {
  return Mips::RA;
}

unsigned MipsRegisterInfo::
getFrameRegister(MachineFunction &MF) const {
  return hasFP(MF) ? Mips::FP : Mips::SP;
}

unsigned MipsRegisterInfo::
getEHExceptionRegister() const {
  assert(0 && "What is the exception register");
  return 0;
}

unsigned MipsRegisterInfo::
getEHHandlerRegister() const {
  assert(0 && "What is the exception handler register");
  return 0;
}

int MipsRegisterInfo::
getDwarfRegNum(unsigned RegNum, bool isEH) const {
  assert(0 && "What is the dwarf register number");
  return -1;
}

`#include "'__arch__`GenRegisterInfo.inc"'

