//==-- TemplateManager.cpp
// Uses template files (living in ../template) to produce code to the
// LLVM backend. Manages data necessary to transform template into 
// specific code to the target architecture.


#include "TemplateManager.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>

using namespace backendgen;

// Creates the M4 file that defines macros expansions to be
// performed in template files. These expansions are defines as
// target specific values, so there is no point calling this function
// if all the data needed is not available yet.
void TemplateManager::CreateM4File()
{
  std::string FileName = WorkingDir;
  FileName += "/macro.m4";
  std::ofstream O(FileName.c_str(), std::ios::out);

  // Creates logtable macro
  O << "m4_define(`m4log', `m4_ifelse($1,1,1,m4_ifelse($1,2,1,m4_ifelse($1," <<
    "4,2,m4_ifelse($1,8,3,m4_ifelse($1,16,4,m4_ifelse($1,24,5,m4_ifelse($1," <<
    "32,5,m4_ifelse($1,48,6,m4_ifelse($1,64,6)))))))))')m4_dnl\n";

  // Defines an expansion for each target specific data
  O << "m4_define(`__arch__',`" << ArchName << "')m4_dnl\n"; 
  O << "m4_define(`__nregs__',`" << NumRegs << "')m4_dnl\n"; 
}

// Creates LLVM backend files based on template files feeded with
// target specific data.
void TemplateManager::CreateBackendFiles()
{
  int ret;
  std::string MacroFileName = WorkingDir;
  std::string RegisterInfoIn = "./template/XXXRegisterInfo.td";
  std::string RegisterInfoOut = WorkingDir;

  MacroFileName += "/macro.m4";
  RegisterInfoOut += "/";
  RegisterInfoOut += ArchName;
  RegisterInfoOut += "RegisterInfo.td";

  // First creates our macro file to insert target specific data
  // into templates
  CreateM4File();

  // Creates XXXRegisterInfo.td file
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfoIn +
		" > " + RegisterInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.td\n";
    exit(1);
  }

  // Success
  remove(MacroFileName.c_str());
}
