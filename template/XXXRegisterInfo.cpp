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
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetFrameInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/STLExtras.h"

using namespace llvm;
using namespace __arch__`';

`'__arch__`'RegisterInfo::`'__arch__`'RegisterInfo(const `'__arch__`'Subtarget &ST, 
                                   const TargetInstrInfo &tii)
  : `'__arch__`'GenRegisterInfo(`'__arch__`'::ADJCALLSTACKDOWN, `'__arch__`'::ADJCALLSTACKUP),
    Subtarget(ST), TII(tii) {}

//===----------------------------------------------------------------------===//
// Callee Saved Registers methods 
//===----------------------------------------------------------------------===//
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
MachineInstr* findNearestUseAfter(MachineInstr* MI, unsigned Reg) {
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
      if (I2->findRegisterUseOperandIdx(Reg) != -1 && found) {
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
MachineBasicBlock::iterator MIGetIterator(MachineInstr* MI) {  
  MachineBasicBlock *I = MI->getParent();  
  for (MachineBasicBlock::iterator I2 = I->begin(), E2 = I->end();
      I2 != E2; ++I2) {
    if (&(*I2) == MI) {
      return I2;
    }    
  }  
  assert(0 && "MI not found");
  return I->begin();
}
}

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


void __arch__`'RegisterInfo::adjustStackFrame(MachineFunction &MF) const
{
  MachineFrameInfo *MFI = MF.getFrameInfo();
  __arch__`'FunctionInfo *FI = MF.getInfo<`'__arch__`'FunctionInfo>();
  unsigned StackAlign = MF.getTarget().getFrameInfo()->getStackAlignment();

  unsigned StackSize = MFI->getStackSize();

  // Adjust CPU Callee Saved Registers Area. Registers RA and FP must
  // be saved in this CPU Area there is the need. This whole Area must 
  // be aligned to the default Stack Alignment requirements.
  int StackOffset = 0-StackSize;
  unsigned RegSize = __wordsize__ / 8;
 
  StackOffset = ((StackOffset+StackAlign-1)/StackAlign*StackAlign);


  if (hasFP(MF)) {
    MFI->setObjectOffset(MFI->CreateStackObject(RegSize, RegSize), 
                         StackOffset);
    FI->setFPStackOffset(StackOffset);
    //TopCPUSavedRegOff = StackOffset;
    StackOffset -= RegSize;
  }

  if (MFI->hasCalls()) {
    MFI->setObjectOffset(MFI->CreateStackObject(RegSize, RegSize), 
                         StackOffset);
    FI->setRAStackOffset(StackOffset);
    //TopCPUSavedRegOff = StackOffset;
    StackOffset -= RegSize;
  }
  StackOffset = ((StackOffset+StackAlign-1)/StackAlign*StackAlign);

  // Update frame info
  MFI->setStackSize(0-StackOffset);

 
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
#if 0
  MachineInstr &MI = *I;
  int Size = MI.getOperand(0).getImm();
  if (MI.getOpcode() == __arch__`'::ADJCALLSTACKDOWN)
    Size = -Size;
  if (Size > 0) {
__eliminate_call_frame_pseudo_positive__
  } else if (Size < 0) {
    Size = -Size;
__eliminate_call_frame_pseudo_negative__
  }
#endif
  MBB.erase(I);
}

bool __arch__`'RegisterInfo::
handleLargeOffset(MachineInstr* MI, int spOffset, unsigned AuxReg,
		  MachineBasicBlock::iterator I) const
{
  MachineBasicBlock &MBB = *MI->getParent();
  bool Patched = false;  
  unsigned checkNum;
  bool negative;
  if (spOffset < 0) {
    checkNum = (unsigned)(0 - spOffset);
    negative = true;
  } else {
    checkNum = (unsigned) spOffset;
    negative = false;
  }
  checkNum = checkNum * 2;
  #define getFrameIndex getIndex
  #define isFrameIndex isFI
  // Check if the immediate will fit into the target field
  if ((checkNum & `'__masktgtimm__`') != 0) {
    // Won't fit. Check if this fit at least in a 16bit immediate
    if ((checkNum & ~0xFFFF) != 0 || negative) {
      // Generate code to fetch a 32-bit imm
__change_fi_to_32_bit__
    } else {
      // Generate code to fetch a 16-bit imm
__change_fi_to_16_bit__
    }
  }
  #undef getFrameIndex
  #undef isFrameIndex
  
  return Patched;
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
    
  unsigned i = 0, j = 0;
  while (!MI.getOperand(i).isFI()) {
    ++i;
    assert (i < MI.getNumOperands() 
	    && "Instr doesn't have FrameIndex operand!");
  }
  j = i + 1;
  while (!MI.getOperand(j).isFI()) {
    ++j;
    assert(j < MI.getNumOperands() && 
           "Instr doesn't have FrameIndex operand!");
  }
  
  if (TII.get(MI.getOpcode()).OpInfo[i].RegClass == 0) {
    unsigned temp = i;
    i = j;
    j = temp;
  }
  // Now, i indicates the base register operand, j indicates the
  // immediate offset operand.

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
  int Offset = ((spOffset < 0) ? (stackSize + spOffset) : (spOffset));
  if (!hasFP(MF))
    spOffset = Offset;
  //Offset    += MI.getOperand(i-1).getImm();

  #ifndef NDEBUG
  DOUT << "Offset     : " << Offset << "\n";
  DOUT << "<--------->\n";
  #endif
  unsigned AuxReg;
  if (FrameIndex >= 0 && hasFP(MF))
    AuxReg = getFrameRegister(MF);
  else
    AuxReg = `'__stackpointer_register__`';
  
  if (!handleLargeOffset(&MI, spOffset, AuxReg, II)) {
    MI.getOperand(j).ChangeToImmediate(spOffset);
    MI.getOperand(i).ChangeToRegister(AuxReg, false);  
  }
}

void __arch__`'RegisterInfo::
emitPrologue(MachineFunction &MF) const 
{
  MachineBasicBlock &MBB = MF.front();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  unsigned align = MFI->getMaxAlignment();
  __arch__`'FunctionInfo *FI       = MF.getInfo<`'__arch__`'FunctionInfo>();
  
  adjustStackFrame(MF);

  // Get the number of bytes to allocate from the FrameInfo
  int NumBytes = (int) MFI->getStackSize();
  
  // Round up to next alignment boundary as required by the ABI.
  if (align) {
    NumBytes = (NumBytes + align - 1) & ~(align - 1);
    //NumBytes = -NumBytes;
  }
  MachineBasicBlock::iterator I = MBB.begin();
  int FPOffset = FI->getFPStackOffset();
  FPOffset = NumBytes + FPOffset;
  int RAOffset = FI->getRAStackOffset();
  RAOffset = NumBytes + RAOffset;
__emit_prologue__
 
}

void __arch__`'RegisterInfo::
emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const 
{
  MachineFrameInfo *MFI            = MF.getFrameInfo();
  __arch__`'FunctionInfo *FI       = MF.getInfo<`'__arch__`'FunctionInfo>();

  // Get the number of bytes from FrameInfo
  int NumBytes = (int) MFI->getStackSize();  

  // Get the FI's where RA and FP are saved.
  int FPOffset = FI->getFPStackOffset();
  int RAOffset = FI->getRAStackOffset();
  MachineBasicBlock::iterator I = prior(MBB.end());
    
__emit_epilogue__
}


void __arch__`'RegisterInfo::
processFunctionBeforeFrameFinalized(MachineFunction &MF) const {

}

unsigned __arch__`'RegisterInfo::
getRARegister() const {
  return __ra_register__;
}

unsigned __arch__`'RegisterInfo::
getFrameRegister(MachineFunction &MF) const {
  return __frame_register__;
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

