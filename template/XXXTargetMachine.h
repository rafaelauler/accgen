//===-- __arch__`'TargetMachine.h - Define TargetMachine for __arch__ ---*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file declares the __arch__ specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

`#ifndef '__arch_in_caps__`TARGETMACHINE_H'
`#define '__arch_in_caps__`TARGETMACHINE_H'

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetFrameInfo.h"
`#include "'__arch__`InstrInfo.h"'
`#include "'__arch__`Subtarget.h"'
`#include "'__arch__`ISelLowering.h"'

namespace llvm {

class Module;

class __arch__`'TargetMachine : public LLVMTargetMachine {
  const TargetData DataLayout;       // Calculates type size & alignment
  __arch__`'Subtarget Subtarget;
  __arch__`'TargetLowering TLInfo;
  __arch__`'InstrInfo InstrInfo;
  TargetFrameInfo FrameInfo;
  
protected:
  virtual const TargetAsmInfo *createTargetAsmInfo() const;
  
public:
  __arch__`'TargetMachine(const Module &M, const std::string &FS);

  virtual const __arch__`'InstrInfo *getInstrInfo() const { return &InstrInfo; }
  virtual const TargetFrameInfo  *getFrameInfo() const { return &FrameInfo; }
  virtual const __arch__`'Subtarget   *getSubtargetImpl() const{ return &Subtarget; }
  virtual const __arch__`'RegisterInfo *getRegisterInfo() const {
    return &InstrInfo.getRegisterInfo();
  }
  virtual __arch__`'TargetLowering* getTargetLowering() const {
    return const_cast<`'__arch__`'TargetLowering*>(&TLInfo);
  }
  virtual const TargetData       *getTargetData() const { return &DataLayout; }
  static unsigned getModuleMatchQuality(const Module &M);

  // Pass Pipeline Configuration
  virtual bool addInstSelector(PassManagerBase &PM, bool Fast);
  virtual bool addPreEmitPass(PassManagerBase &PM, bool Fast);
  virtual bool addAssemblyEmitter(PassManagerBase &PM, bool Fast, 
                                  raw_ostream &Out);
};

} // end namespace llvm

#endif
