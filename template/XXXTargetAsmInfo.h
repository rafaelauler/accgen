//=====-- __arch__`'TargetAsmInfo.h - __arch__ asm properties ---------*- C++ -*--====//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the __arch__`'TargetAsmInfo class.
//
//===----------------------------------------------------------------------===//

`#ifndef '__arch_in_caps__`TARGETASMINFO_H'
`#define '__arch_in_caps__`TARGETASMINFO_H'

#include "llvm/Target/TargetAsmInfo.h"
#include "llvm/Target/ELFTargetAsmInfo.h"

namespace llvm {

  // Forward declaration.
  class TargetMachine;

  struct __arch__`'ELFTargetAsmInfo : public ELFTargetAsmInfo {
    explicit __arch__`'ELFTargetAsmInfo(const TargetMachine &TM);

    std::string printSectionFlags(unsigned flags) const;
  };

} // namespace llvm

#endif
