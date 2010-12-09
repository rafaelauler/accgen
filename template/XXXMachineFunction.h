//===-- __arch__`'MachineFunctionInfo.h - Private data used for __arch__ ----*- C++ -*-=//
//
//                 The ArchC LLVM Backend Generator
//
// Automatically generated file. 
//
//===----------------------------------------------------------------------===//
//
// This file declares the __arch__ specific subclass of MachineFunctionInfo.
//
//===----------------------------------------------------------------------===//

`#ifndef '__arch_in_caps__`_MACHINE_FUNCTION_INFO_H'
`#define '__arch_in_caps__`_MACHINE_FUNCTION_INFO_H'

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/VectorExtras.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Mangler.h"

namespace llvm {

class GlobalValue;

struct InvalidOEIndexException {};
  
/// __arch__`'FunctionInfo - This class is derived from MachineFunction private
/// __arch__ target-specific information for each MachineFunction.
class __arch__`'FunctionInfo : public MachineFunctionInfo {

private:
  /// Holds for each function where on the stack the Frame Pointer must be 
  /// saved. This is used on Prologue and Epilogue to emit FP save/restore
  int FPStackOffset;

  /// Holds for each function where on the stack the Return Address must be 
  /// saved. This is used on Prologue and Epilogue to emit RA save/restore
  int RAStackOffset;
  
  struct GlobalValueElement {
    const GlobalValue* gv;
    int index;
    GlobalValueElement(const GlobalValue* gv, int index):
      gv(gv), index(index) {}
  };
  
  SmallVector<GlobalValueElement, 16> GlobalValues;
  
  struct GVOperandExpression {
    int index;
    std::string Expression;
    GVOperandExpression(const std::string& N, int index):
      index(index), Expression(N) {}
  };
  
  SmallVector<GVOperandExpression, 16> OperandExpressions;

  std::string DefaultExp;
public:
  __arch__`'FunctionInfo(MachineFunction& MF) 
  : FPStackOffset(0), RAStackOffset(0), DefaultExp("GVGOESHERE")
  {}

  int getFPStackOffset() const { return FPStackOffset; }
  void setFPStackOffset(int Off) { FPStackOffset = Off; }

  int getRAStackOffset() const { return RAStackOffset; }
  void setRAStackOffset(int Off) { RAStackOffset = Off; }
  
  void insertNewGlobalValue(const GlobalValue* gv) {
    int index = 0;
    if (GlobalValues.size() > 0)      
      index = GlobalValues.back().index + 1;
    
    int found = getIndex(gv);
    if (found == -1) {
      GlobalValues.push_back(GlobalValueElement(gv, index));
    }
  }
  
  int insertNewOperandExpression(const std::string &N) {
    int index = 1;
    if (OperandExpressions.size() > 0)
      index = OperandExpressions.back().index + 1;
    OperandExpressions.push_back(GVOperandExpression(N, index));
    return index;
  }
  
  const std::string& getOperandExpression(int index) const {
    for (unsigned i = 0, e = OperandExpressions.size(); i != e; ++i) {
      if (OperandExpressions[i].index == index)
	return OperandExpressions[i].Expression;
    }
    return DefaultExp;
  }
  
  int getIndex(const GlobalValue* gv) const {
    int index = -1;
    for (unsigned i = 0, e = GlobalValues.size(); i != e; ++i) {
      if (GlobalValues[i].gv == gv) {
	index = GlobalValues[i].index;
	break;
      }
      if (i == e) {
	return -1;
      }
    }
    return index;
  }
  void printGlobalValues(raw_ostream &O, Mangler* Mang) const {
    for (unsigned i = 0, e = GlobalValues.size(); i != e; ++i) {
      O << ".word " << Mang->getValueName(GlobalValues[i].gv) << "\n";      
    }
  }
/*
  void recordLoadArgsFI(int FI, int SPOffset) {
    if (!HasLoadArgs) HasLoadArgs=true;
    FnLoadArgs.push_back(MipsFIHolder(FI, SPOffset));
  }
  void recordStoreVarArgsFI(int FI, int SPOffset) {
    if (!HasStoreVarArgs) HasStoreVarArgs=true;
    FnStoreVarArgs.push_back(MipsFIHolder(FI, SPOffset));
  }

  void adjustLoadArgsFI(MachineFrameInfo *MFI) const {
    if (!hasLoadArgs()) return;
    for (unsigned i = 0, e = FnLoadArgs.size(); i != e; ++i) 
      MFI->setObjectOffset( FnLoadArgs[i].FI, FnLoadArgs[i].SPOffset );
  }
  void adjustStoreVarArgsFI(MachineFrameInfo *MFI) const {
    if (!hasStoreVarArgs()) return; 
    for (unsigned i = 0, e = FnStoreVarArgs.size(); i != e; ++i) 
      MFI->setObjectOffset( FnStoreVarArgs[i].FI, FnStoreVarArgs[i].SPOffset );
  }
*/
};

} // end of namespace llvm

#endif // MIPS_MACHINE_FUNCTION_INFO_H
