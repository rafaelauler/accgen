//===- CMemWatcher.cpp - Implementation for Memory Watcher--*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "CMemWatcher.h"
extern "C" {
#include <malloc.h>
}

using namespace helper;

CMemWatcher* CMemWatcher::pInstance = NULL;

// Singleton instance access point
CMemWatcher* CMemWatcher::Instance() {
  if (pInstance == NULL) {
    pInstance = new CMemWatcher();
  }
  return pInstance;
}

void CMemWatcher::Destroy() {
  if (pInstance != NULL)
    delete pInstance;
}

// Data storage used by hooks
static void** AllPointers;
static unsigned CurSize;
static unsigned CurPointer;
static void (*old_free_hook) (void *__ptr, const void *caller);
static void *(*old_malloc_hook) (size_t __size, const void *caller);
const static unsigned STEP = 10000;

// Hooks implementation
static void my_free_hook (void *ptr, const void *caller);

static void *my_malloc_hook (size_t size, const void *caller) {
  void *result;
  // Restore old hooks
  __free_hook = old_free_hook;
  __malloc_hook = old_malloc_hook;
  // Call recursively
  result = malloc(size);
  // Stores the pointer
  if (CurPointer == CurSize) {
    CurSize += STEP;
    AllPointers = (void **) realloc(AllPointers, sizeof(void*)*CurSize);
  }
  AllPointers[CurPointer++] = result;
  // Save underlying hooks
  old_free_hook = __free_hook;
  old_malloc_hook = __malloc_hook;
  // Restores our hooks
  __free_hook = my_free_hook;
  __malloc_hook = my_malloc_hook;
  return result;
}

static void my_free_hook (void *ptr, const void *caller) {
  // Restore old hooks
  __free_hook = old_free_hook;
  __malloc_hook = old_malloc_hook;
  // Call recursively
  free(ptr);
  // Save underlying hooks
  old_free_hook = __free_hook;
  old_malloc_hook = __malloc_hook;
  // Removes this pointer from our tracking
  for (int i = 0; i < CurPointer; ++i) {
    if (AllPointers[i] == ptr) {
      for (int i2 = i; i2 < CurPointer-1; ++i2) {
	AllPointers[i2] = AllPointers[i2+1];
      }
      --CurPointer;
      break;
    }
  }
  // Restores our hooks
  __free_hook = my_free_hook;
  __malloc_hook = my_malloc_hook;
  return;
}

// CMemWatcher member functions implementation

void CMemWatcher::InstallHooks() {
  AllPointers = (void **)malloc(sizeof(void*)*STEP);
  CurSize = STEP;
  CurPointer = 0;
  old_free_hook = __free_hook;
  old_malloc_hook = __malloc_hook;
  __free_hook = my_free_hook;
  __malloc_hook = my_malloc_hook;
}

void CMemWatcher::ReportStatistics(std::ostream &S) {
  S << "Total pointers to allocated regions: " << CurPointer
    << "\n";
}

void CMemWatcher::UninstallHooks() {
  // Restore old hooks (uninstall)
  __free_hook = old_free_hook;
  __malloc_hook = old_malloc_hook;
}

void CMemWatcher::FreeAll() {
  // Deallocates everything
  for (int i = 0; i < CurPointer; ++i) {
    free(AllPointers[i]);
  }
  // Deallocated our data structures
  free(AllPointers);
}
