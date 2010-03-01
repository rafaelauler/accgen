//===- CMemWatcher.h - header file for C Memory Watcher   --*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include <iostream>

namespace helper {

  class CMemWatcher {
    // Singleton implementation
    static CMemWatcher* pInstance;
    CMemWatcher() {} // prevent clients from creating a new instance
    CMemWatcher(const CMemWatcher&); // prevent clients from copying
  public:
    static CMemWatcher* Instance();
    static void Destroy();
    void InstallHooks();
    void ReportStatistics(std::ostream &S);
    void UninstallHooks();
    void FreeAll();
  };

}
