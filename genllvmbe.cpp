#include <iostream>
#include <fstream>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "InsnFormat.h"
#include "Insn.h"
#include "ArchEmitter.h"
#include "TemplateManager.h"
#include <map>

extern "C" { 
  #include "acpp.h"
}

char *file_name = NULL; /* name of the main ArchC file */
char *isa_filename_with_path = NULL;

using namespace backendgen;

void parse_archc_description(char **argv) {
  /* Initializes the pre-processor */
  acppInit(1);
  file_name = (char *)malloc(strlen(argv[1]) + strlen(argv[2]) + 1);
  strcpy(file_name, argv[1]);
  strcpy(file_name + strlen(argv[1]), argv[2]);
  
  /* Parse the ARCH file */
  if (!acppLoad(file_name)) {
    fprintf(stderr, "Invalid file: '%s'.\n", file_name);
    exit(1);
  }

  if (acppRun()) {
    fprintf(stderr, "Parser error in ARCH file.\n");
    exit(1);
  }
  acppUnload();

  /* Parse the ISA file */
  isa_filename_with_path = (char *)malloc(strlen(argv[1]) + strlen(isa_filename) + 1);
  strcpy(isa_filename_with_path, argv[1]);
  strcpy(isa_filename_with_path + strlen(argv[1]), isa_filename);
  
  if (!acppLoad(isa_filename_with_path)) {
    
    fprintf(stderr, "Invalid ISA file: '%s'.\n", isa_filename);
    exit(1);
  }
  if (acppRun()) {
    fprintf(stderr, "Parser error in ISA file.\n");
    exit(1);
  }
  acppUnload();

  free(file_name);
  free(isa_filename_with_path);
}

void print_formats() {
  ac_dec_format* pformat;
  ac_dec_field* pfield;
  for (pformat = format_ins_list; pformat != NULL; pformat = pformat->next) {
    std::cout << pformat->name << ", size:" << pformat->size << std::endl;
    for (pfield = pformat->fields; pfield != NULL; pfield = pfield->next) {
      std::cout << "  " << pfield->name 
                << ", id:" << pfield->id
                << ", size:" << pfield->size
                << ", first_bit:" << pfield->first_bit
                << std::endl;
    }
  }
}

std::map<std::string, InsnFormat*> FormatMap;
typedef std::map<std::string, InsnFormat*>::iterator FormatMapIter;

void DebugFormats() {
  for (FormatMapIter I = FormatMap.begin(), E = FormatMap.end(); I != E; ++I) {
    InsnFormat *IF = I->second;
    std::cout << IF->getName() 
              << ", size:" << IF->getSizeInBits() 
              << ", baseclass: " << IF->getBaseLLVMFormatClassName()
              << std::endl;
    std::vector<FormatField *> FF = IF->getFields();
    for (std::vector<FormatField *>::iterator FI = FF.begin(), FE = FF.end(); 
         FI != FE; ++FI) {
      FormatField *Field = *FI;
      std::cout << "  " <<  Field->getName() 
                << ", size:" << Field->getSizeInBits()
                << ", (" << Field->getStartBitPos()
                << "," << Field->getEndBitPos() << ")"
                << ", grpnum=" << Field->getGroupId()
                << ","
                << std::endl;
    }
  }
}

std::vector<InsnFormat *> BaseFormatNames;

void BuildFormats() {
  ac_dec_format* pformat;
  ac_dec_field* pfield;

  // Fulfill all instruction formats with their fields.
  for (pformat = format_ins_list; pformat != NULL; pformat = pformat->next) {
    InsnFormat *IF = new InsnFormat(pformat->name, pformat->size);

    // XXX: Hardcoded for big endian
    unsigned startbit, endbit;
    for (pfield = pformat->fields; pfield != NULL; pfield = pfield->next) {
      endbit = pformat->size - pfield->first_bit - 1;
      startbit = endbit + pfield->size - 1;
      IF->addField(new FormatField(pfield->name, pfield->size, startbit, endbit));
    }
    FormatMap[pformat->name] = IF;
  }

  // Only looks into the first field to create a base
  // class for that kind of format. This could be improved to detect as much
  // as possible for each format. That is, all insn formats with the first 
  // field "op:5" will belong to format class A, and those with "opext:5" to
  // class B. An architecture usually has only one base class.
  std::vector<const char *> FirstFields;
  for (FormatMapIter I = FormatMap.begin(), E = FormatMap.end(); I != E; ++I) {
    InsnFormat *IF = I->second;
    bool HasEntry = false;

    for (int i = 0, e = FirstFields.size(); i != e; ++i) {
      if (!strcmp(IF->getField(0)->getName(), FirstFields[i])) {
        HasEntry = true;
        IF->setBaseLLVMFormatClassName(i);
      }
    }

    if (!HasEntry) {
      FirstFields.push_back(IF->getField(0)->getName());
      IF->setBaseLLVMFormatClassName(FirstFields.size()-1);
      BaseFormatNames.push_back(IF);
    }

    IF->recognizeGroups();
  }

  //DebugFormats();
}

InsnIdMapTy InsnIdMap;

void DebugInsn() {
  for (InsnIdMapIter IM = InsnIdMap.begin(), EM = InsnIdMap.end(); IM != EM; ++IM) {
    std::vector<Insn *> VI = IM->second;
    for (std::vector<Insn *>::iterator II = VI.begin(), IE = VI.end(); II != IE; ++II) {
      Insn *I = *II;
      std::cout << I->getName()
                << ", " << IM->first
                << ", " << I->getFormat()->getName()
                << ", " << I->getOperandsFmts()
                << " "; //std::endl;
      std::cout << I->getFmtStr() << std::endl;
      for (int i = 0, e = I->getNumOperands(); i != e; ++i) {
        std::cout << "  " << I->getOperand(i)->getName() << "(";
        for (int fi = 0, fe = I->getOperand(i)->getNumFields(); fi != fe; ++fi) { 
          FormatField *FF = I->getOperand(i)->getField(fi);
          std::cout << FF->getName()
                    << ":" << FF->getSizeInBits()
                    << ",";
        }
        std::cout << ")" << std::endl;
      }
    }
  }
}

void BuildInsn() {
  ac_asm_insn *pinsn = ac_asm_get_asm_insn_list();
  ac_operand_list *operand;
  ac_asm_insn_field *field;
  ac_dec_instr *insn;

  for (; pinsn != NULL; pinsn = pinsn->next) {

    Insn *I = new Insn(pinsn->mnemonic, 
                       pinsn->op_literal, 
                       FormatMap[pinsn->insn->format]);

    // XXX: Hardcoded for big endian
    unsigned startbit, endbit;
    for (operand = pinsn->operands; operand != NULL; operand = operand->next) {
      InsnOperand *IO = new InsnOperand(operand->str);
      for (field = operand->fields; field != NULL; field = field->next) {
        endbit = pinsn->insn->size - field->first_bit - 1;
        startbit = endbit + field->size - 1;
        IO->addField(new FormatField(field->name, field->size, startbit, endbit));
      }
      I->addOperand(IO);
    }
    I->parseOperandsFmts();

    //if (InsnIdMap.find(pinsn->insn->id) == InsnIdMap.end())
    InsnIdMap[pinsn->insn->id].push_back(I);
  }

  DebugInsn();
}

void print_insns() {
  ac_asm_insn *pinsn = ac_asm_get_asm_insn_list();
  ac_operand_list *operand;
  ac_asm_insn_field *field;
  ac_dec_instr *insn;
  for (; pinsn != NULL; pinsn = pinsn->next) {
    if (pinsn->insn == NULL) {
      std::cout << pinsn->mnemonic
		<< ", * pseudo *" 
      		<< ", " << pinsn->op_literal
		<< std::endl;  
    } else {
      std::cout << pinsn->mnemonic
		<< ", " << pinsn->insn->id 
		<< ", " << pinsn->insn->format 
		<< ", " << pinsn->op_literal
		<< std::endl;  
    }
    for (operand = pinsn->operands; operand != NULL; operand = operand->next) {
      std::cout << "  " << operand->str << "(";
      for (field = operand->fields; field != NULL; field = field->next) {
        std::cout << field->name 
        << ":" << field->size 
        << "," << field->id << ",";
      }
      std::cout << ")" << std::endl;
    }
  }
}

void create_dir(const char *path) {
  struct stat sb;
  if (stat(path, &sb) == -1) {
    mkdir(path, 0777); 
  }
}

int main(int argc, char **argv) {
  parse_archc_description(argv);
  //print_formats();  
  //print_insns();
  const char *TmpDir = "llvmbackend";
  create_dir(TmpDir);

  BuildFormats();
  BuildInsn();

  // Create the LLVM Instruction Formats file for Sparc16
  const char *FormatsFile = "llvmbackend/Sparc16InstrFormats.td";
  std::ofstream O;
  ArchEmitter AEmitter = ArchEmitter();
  O.open(FormatsFile, std::ios::out | std::ios::trunc); 
  AEmitter.EmitInstrutionFormatClasses(FormatMap, BaseFormatNames, O);
  O.close();

  // Create LLVM backend files based on template files
  TemplateManager TM;
  TM.SetArchName("sparc16");
  TM.SetNumRegs(32);
  TM.SetWorkingDir(TmpDir);
  TM.CreateBackendFiles();

  return 0;
}
