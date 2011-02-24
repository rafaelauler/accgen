set(LLVM_TARGET_DEFINITIONS `'__arch__`'.td)

tablegen(`'__arch__`'GenRegisterInfo.h.inc -gen-register-desc-header)
tablegen(`'__arch__`'GenRegisterNames.inc -gen-register-enums)
tablegen(`'__arch__`'GenRegisterInfo.inc -gen-register-desc)
tablegen(`'__arch__`'GenInstrNames.inc -gen-instr-enums)
tablegen(`'__arch__`'GenInstrInfo.inc -gen-instr-desc)
tablegen(`'__arch__`'GenAsmWriter.inc -gen-asm-writer)
tablegen(`'__arch__`'GenDAGISel.inc -gen-dag-isel)
tablegen(`'__arch__`'GenCallingConv.inc -gen-callingconv)
tablegen(`'__arch__`'GenSubtarget.inc -gen-subtarget)

add_llvm_target(`'__arch__`'
  __arch__`'AsmPrinter.cpp
  __arch__`'InstrInfo.cpp
  __arch__`'ISelDAGToDAG.cpp
  __arch__`'ISelLowering.cpp
  __arch__`'RegisterInfo.cpp
  __arch__`'Subtarget.cpp
  __arch__`'TargetAsmInfo.cpp
  __arch__`'TargetMachine.cpp
	__arch__`'DelaySlotFiller.cpp
  )
