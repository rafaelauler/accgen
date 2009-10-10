//==-- TemplateManager.h
// Use template files (living in ../template) to produce code to the
// LLVM backend. Manage data necessary to transform template into 
// specific code to the target architecture.

#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

//==-- includes --==//
#include <stdlib.h>

//==-- Class prototypes --==//

namespace backendgen {

class TemplateManager {
  // Target specific data used when specializing templates for a 
  // given architecture
  const char * ArchName;
  int NumRegs;
  // Working directory where macro files are created and output
  // is stored.
  const char * WorkingDir;
  
  // Private members
  void CreateM4File();
  
 public:
  explicit TemplateManager(): ArchName(NULL), NumRegs(0), WorkingDir(NULL) {}
  ~TemplateManager() {}

  void SetArchName (const char * name) { ArchName = name; }
  void SetNumRegs (int nregs) { NumRegs = nregs; }
  void SetWorkingDir (const char * wdir) { WorkingDir = wdir; }

  void CreateBackendFiles();

  
};

}

#endif
