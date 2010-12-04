//===- __arch__`'RegisterInfo.h - __arch__ Register Information Impl ------*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file contains the __arch__ implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

`#ifndef '__arch_in_caps__`REGISTERINFO_H'
`#define '__arch_in_caps__`REGISTERINFO_H'

`#include "'__arch__`.h"'
#include "llvm/Target/TargetRegisterInfo.h"
`#include "'__arch__`GenRegisterInfo.h.inc"'

namespace llvm {
class __arch__`'Subtarget;
class TargetInstrInfo;
class Type;

struct __arch__`'RegisterInfo : public __arch__`'GenRegisterInfo {
  const __arch__`'Subtarget &Subtarget;
  const TargetInstrInfo &TII;
  
  __arch__`'RegisterInfo(const __arch__`'Subtarget &Subtarget, const TargetInstrInfo &tii);

  /// Code Generation virtual methods...
  const unsigned *getCalleeSavedRegs(const MachineFunction* MF = 0) const;

  const TargetRegisterClass* const*
  getCalleeSavedRegClasses(const MachineFunction* MF = 0) const;

  BitVector getReservedRegs(const MachineFunction &MF) const;

  bool hasFP(const MachineFunction &MF) const;

  void eliminateCallFramePseudoInstr(MachineFunction &MF,
                                     MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I) const;

  /// Stack Frame Processing Methods
  bool handleLargeOffset(MachineInstr* MI, int spOffset, unsigned AuxReg,
			 MachineBasicBlock::iterator I) const;
  void eliminateFrameIndex(MachineBasicBlock::iterator II,
                           int SPAdj, RegScavenger *RS = NULL) const;

  void processFunctionBeforeFrameFinalized(MachineFunction &MF) const;

  void emitPrologue(MachineFunction &MF) const;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const;
  
  /// Debug information queries.
  unsigned getRARegister() const;
  unsigned getFrameRegister(MachineFunction &MF) const;

  /// Exception handling queries.
  unsigned getEHExceptionRegister() const;
  unsigned getEHHandlerRegister() const;

  int getDwarfRegNum(unsigned RegNum, bool isEH) const;
};

} // end namespace llvm

#endif
