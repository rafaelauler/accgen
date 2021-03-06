//===-- __arch__`ISelLowering.h' - __arch__ DAG Lowering Interface ------*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that __arch__ uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

`#ifndef '__arch_in_caps__`_ISELLOWERING_H'
`#define '__arch_in_caps__`_ISELLOWERING_H'

#include "llvm/Target/TargetLowering.h"
`#include "'__arch__`.h"'

namespace llvm {
  namespace __arch_in_caps__`'ISD {
    enum {
      FIRST_NUMBER = ISD::BUILTIN_OP_END,
      CMPICC,      // Compare two GPR operands, set icc.
      CMPFCC,      // Compare two FP operands, set fcc.
      BRICC,       // Branch to dest on icc condition
      BRFCC,       // Branch to dest on fcc condition
      SELECT_ICC,  // Select between two values using the current ICC flags.
      SELECT_FCC,  // Select between two values using the current FCC flags.

      Hi, Lo,      // Hi/Lo operations, typically on a global address.

      FTOI,        // FP to Int within a FP register.
      ITOF,        // Int to FP within a FP register.

      CALL,        // A call instruction.
      Select_CC,
      DummyReference,
      RET_FLAG     // Return with a flag operand.
    };
  }

  class __arch__`'TargetLowering : public TargetLowering {
    int VarArgsFrameOffset;   // Frame offset to start of varargs area.

    SDNode *LowerCallResult(SDValue Chain, SDValue InFlag, CallSDNode *TheCall, 
        unsigned CallingConv, SelectionDAG &DAG);
    SDValue LowerCALL(SDValue Op, SelectionDAG &DAG);
    SDValue LowerGLOBALADDRESS(SDValue Op, SelectionDAG &DAG);
    SDValue LowerCONSTANTPOOL(SDValue Op, SelectionDAG &DAG);
    SDValue LowerFORMAL_ARGUMENTS(SDValue Op, SelectionDAG &DAG);
    SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG);

  public:
    __arch__`'TargetLowering(TargetMachine &TM);
    virtual SDValue LowerOperation(SDValue Op, SelectionDAG &DAG);

    int getVarArgsFrameOffset() const { return VarArgsFrameOffset; }

    /// computeMaskedBitsForTargetNode - Determine which of the bits specified
    /// in Mask are known to be either zero or one and return them in the
    /// KnownZero/KnownOne bitsets.
    virtual void computeMaskedBitsForTargetNode(const SDValue Op,
                                                const APInt &Mask,
                                                APInt &KnownZero,
                                                APInt &KnownOne,
                                                const SelectionDAG &DAG,
                                                unsigned Depth = 0) const;    

    virtual MachineBasicBlock *EmitInstrWithCustomInserter(MachineInstr *MI,
                                                        MachineBasicBlock *MBB);

    virtual const char *getTargetNodeName(unsigned Opcode) const;

    ConstraintType getConstraintType(const std::string &Constraint) const;
    std::pair<unsigned, const TargetRegisterClass*>
    getRegForInlineAsmConstraint(const std::string &Constraint, MVT VT) const;
    std::vector<unsigned>
    getRegClassForInlineAsmConstraint(const std::string &Constraint,
                                      MVT VT) const;

    virtual bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const;
  };
} // end namespace llvm

`#endif    //' __arch_in_caps__`'_ISELLOWERING_H
