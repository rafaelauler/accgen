
#include "Instruction.h"
#include "InsnFormat.h"
#include "ArchEmitter.h"
#include <map>
#include <string.h>

using namespace backendgen; 

typedef std::map<std::string, InsnFormat*> FormatMapTy;
typedef std::map<std::string, InsnFormat*>::iterator FormatMapIterTy;

void ArchEmitter::EmitDeclareBitFieldMatch(FormatField &FF, std::ofstream &O) {
  O << "  let Inst{" 
    << FF.getStartBitPos() << "-" << FF.getEndBitPos() << "}" 
    << " = " << FF.getName() << ";" << std::endl;
}

void ArchEmitter::EmitDeclareBitField(FormatField &FF, std::ofstream &O) {
  O << "  bits<" 
    << FF.getSizeInBits() 
    << "> " << FF.getName() << ";" << std::endl;
}

void ArchEmitter::EmitDeclareClassArgs(InsnFormat &IF, std::ofstream &O,
                                       unsigned GrpNum) {
  // Get all fields which start with "op" and use them as argument. We
  // drop the first one, since its already declared on a parent.
  std::vector<FormatField*> FF = IF.getFields();
  for (int i = 1, e = FF.size(); i != e; ++i) {
    if ((FF[i]->getGroupId() == GrpNum || FF[i]->getGroupId() == 0) &&
        (!strncmp(FF[i]->getName(), "op", 2) || 
        !strncmp(FF[i]->getName(), "cond", 4) ||
        !strncmp(FF[i]->getName(), "t", 1))) {
      O << "bits<" 
        << FF[i]->getSizeInBits() << "> " 
        << FF[i]->getName() << "Val, ";
    }
  }
}

void ArchEmitter::EmitDeclareMatchAttr(InsnFormat &IF, std::ofstream &O,
                                       unsigned GrpNum) {
  // Get all fields which start with "op" and use them as argument. We
  // drop the first one, since its already declared on a parent.
  std::vector<FormatField*> FF = IF.getFields();
  for (int i = 1, e = FF.size(); i != e; ++i) {
    if ((FF[i]->getGroupId() == GrpNum || FF[i]->getGroupId() == 0) &&
        (!strncmp(FF[i]->getName(), "op", 2) || 
         !strncmp(FF[i]->getName(), "cond", 4) ||
         !strncmp(FF[i]->getName(), "t", 1))) {
      O << "  let " << FF[i]->getName() 
        << " = " << FF[i]->getName() << "Val; " << std::endl;
    }
  }
}

void ArchEmitter::EmitFormatClass(InsnFormat &IF, std::ofstream &O, unsigned GrpNumber) {
  // Class declarations
  O << std::endl << "class " << IF.getName(GrpNumber) << "<";

  // Emit the parameters
  EmitDeclareClassArgs(IF, O, GrpNumber);

  // Emit the inheritance from the base format classes
  O << "dag outs, dag ins, string asmstr, list<dag> pattern>" << std::endl 
    << "   : " << IF.getBaseLLVMFormatClassName() 
    << "<outs, ins, asmstr, pattern> {" << std::endl << std::endl;

  // Emit the remaining fields 
  std::vector<FormatField*> FF = IF.getFields();
  for (int i = 1, e = FF.size(); i != e; ++i) {
    if (FF[i]->getGroupId() == GrpNumber || FF[i]->getGroupId() == 0)
      EmitDeclareBitField(*FF[i], O); 
  }
  O << std::endl;

  // Map the common first field into the Format bits
  for (int i = 1, e = FF.size(); i != e; ++i) {
    if (FF[i]->getGroupId() == GrpNumber || FF[i]->getGroupId() == 0)
      EmitDeclareBitFieldMatch(*FF[i], O);
  }
  O << std::endl;

  // Emit the match between the values passed as params and
  // the bit position where they belong
  EmitDeclareMatchAttr(IF, O, GrpNumber);

  O << "}" << std::endl;
}

void ArchEmitter::EmitInstrutionFormatClasses(FormatMapTy FormatMap, 
                                              std::vector<InsnFormat*> BaseFormatName,
                                              std::ofstream &O) {

  // Emit declarations for all base classes!
  for (std::vector<InsnFormat*>::iterator I = BaseFormatName.begin(), 
       E = BaseFormatName.end(); I != E; ++I) {
    InsnFormat &IF = *(*I);

    // Class declarations
    O << "class " << IF.getBaseLLVMFormatClassName()
      << "<dag outs, dag ins, string asmstr, list<dag> pattern> : Instruction {"
      << std::endl

      // The instruction format size declaration
      << "  field bits<" << IF.getSizeInBits() << "> Inst;" << std::endl
      << "  let Namespace = \"SP16\";" << std::endl << std::endl;
      
    // Declare the common first field
    EmitDeclareBitField(*IF.getField(0), O);

    // Map the common first field into the Format bits
    EmitDeclareBitFieldMatch(*IF.getField(0), O);

    O << std::endl
      << "  dag OutOperandList = outs;" << std::endl
      << "  dag InOperandList = ins;" << std::endl
      << "  let AsmString = asmstr;" << std::endl
      << "  let Pattern = pattern;" << std::endl
      << "}" << std::endl << std::endl;
  }

  // Emit declarations for all instruction formats
  for (FormatMapIterTy I = FormatMap.begin(), E = FormatMap.end(); 
       I != E; ++I) {
    InsnFormat &IF = *(I->second);
    
    if (!IF.getNumGroups()) {
      EmitFormatClass(IF, O, 0);
      continue;
    }

    // Handle groups: e.g. [ xx:3 yy:4 | zz:4 vx:3 ]
    for (int NumGrp = 0, NumGrpEnd = IF.getNumGroups(); 
         NumGrp != NumGrpEnd; ++NumGrp) {
      unsigned GrpNumber = NumGrp+1;
      EmitFormatClass(IF, O, GrpNumber);
    }
  }
}

//void ArchEmitter::hasOperandType(std::vector<Insn *> VI, std::string OpTy) {
//  
//}
//
//void ArchEmitter::chooseInstructionToEmit(std::vector<Insn *> VI) {
//
//  // Try to emit a instruction without %lo, %hi, ...
//  for (std::vector<Insn *>::iterator II = VI.begin(), 
//       IE = VI.end(); II != IE; ++II) {
//    Insn *I = *II;
//    for (int i = 0, e = I->getNumOperands(); i != e; ++i) {
//      std::string &Name = I->getOperand(i)->getName();
//      Name.find("")
//    }
//  }
//  else if hasOperandType(VI, "imm")
//    re
//
//  return VI[0];
//}
//
//void ArchEmitter::EmitInstructions(InsnIdMapTy &IIM, std::ofstream &O) {
//  for (InsnIdMapIter IM = IIM.begin(), EM = IIM.end(); IM != EM; ++IM) {
//
//    std::vector<Insn *> VI = IM->second;
//    Insn *I = chooseInstructionToEmit(VI);
//  
//    std::cout << I->getName()
//              << ", " << IM->first
//              << ", " << I->getFormat()->getName()
//              << ", " << I->getOperandsFmts()
//              << std::endl;
//    for (int i = 0, e = I->getNumOperands(); i != e; ++i) {
//      std::cout << "  " << I->getOperand(i)->getName() << "(";
//      for (int fi = 0, fe = I->getOperand(i)->getNumFields(); fi != fe; ++fi) { 
//        FormatField *FF = I->getOperand(i)->getField(fi);
//        std::cout << FF->getName()
//                  << ":" << FF->getSizeInBits()
//                  << ",";
//      }
//      std::cout << ")" << std::endl;
//    }
//  }
//}
