//===- __arch__`InstrInfo.td' - __arch__ Instruction defs --*- tablegen -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include `"'__arch__`InstrFormats.td"'

//===----------------------------------------------------------------------===//
// Type profiles
//===----------------------------------------------------------------------===//
def SDT_Ret      : SDTypeProfile<0, 1, [SDTCisInt<0>]>;
def SDT_Call     : SDTypeProfile<0, 1, [SDTCisVT<0, iPTR>]>;
def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>]>;
def SDT_CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_selectcc :  SDTypeProfile<1, 5, [SDTCisSameAs<0, 1>, 
                     SDTCisSameAs<1, 2>, SDTCisSameAs<2, 3>, 
                     SDTCisSameAs<3, 4>, SDTCisVT<4, i32>]>;

//===----------------------------------------------------------------------===//
// Target specific nodes
//===----------------------------------------------------------------------===//
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart,
                           [SDNPHasChain, SDNPOutFlag]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_CallSeqEnd,
                           [SDNPHasChain, SDNPOptInFlag, SDNPOutFlag]>;
def call : SDNode<"`'__arch_in_caps__`'ISD::CALL",SDT_Call, [SDNPHasChain,
                         SDNPOutFlag]>;
def selecticc : SDNode<"`'__arch_in_caps__`'ISD::Select_CC", SDT_selectcc,
                       []>;
// This operand is used in our MachineDAG to represent literals to be coded
// in the target assembly language. It corresponds to "IsLiteral" flag
// used in the PatternTranslator class of the LLVM backend generator.
// An Operand instance is used in instructions operands (ins or outs dags) to
// determine the types and how to print the operands in assembly form.
def tgliteral : Operand<i32> {
  let PrintMethod = "printLiteral";
  let MIOperandInfo = (ops i32imm); // No operands
}

// Complex pattern "FrameIndex" used to extract the frameindex number
def CPFI : ComplexPattern<i32, 1, "SelectCPFI", [], []>;

//===----------------------------------------------------------------------===//
// __arch__ instruction definitions
//===----------------------------------------------------------------------===//

__instructions_definitions__

//===----------------------------------------------------------------------===//
// __arch__ pseudo instruction definitions
//===----------------------------------------------------------------------===//
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
   : InstSP16_0<outs, ins, asmstr, pattern>;


def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt),
                               "!ADJCALLSTACKDOWN $amt",
                               [(callseq_start timm:$amt)]>;
def ADJCALLSTACKUP : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                            "!ADJCALLSTACKUP $amt1",
                            [(callseq_end timm:$amt1, timm:$amt2)]>;
			    
let usesCustomDAGSchedInserter = 1 in {   // Expanded by the scheduler.
__select_cc_patts__  
}

//===----------------------------------------------------------------------===//
// Non-Instruction Patterns
//===----------------------------------------------------------------------===//



// Small immediates.
//def : Pat<(i32 simm13:$val),
  //        (ORri G0, imm:$val)>;
