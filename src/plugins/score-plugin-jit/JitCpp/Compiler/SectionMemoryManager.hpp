#pragma once

#include <llvm/ExecutionEngine/SectionMemoryManager.h>

class SingleSectionMemoryManager : public llvm::SectionMemoryManager
{
  struct Block
  {
    uint8_t *Addr = nullptr, *End = nullptr;
    void Reset(uint8_t* Ptr, uintptr_t Size);
    uint8_t* Next(uintptr_t Size, unsigned Alignment);
  };
  Block Code, ROData, RWData;

public:
  uint8_t* allocateCodeSection(
      uintptr_t Size, unsigned Align, unsigned ID, llvm::StringRef Name) final;

  uint8_t* allocateDataSection(
      uintptr_t Size, unsigned Align, unsigned ID, llvm::StringRef Name, bool RO) final;

  void reserveAllocationSpace(
      uintptr_t CodeSize, uint32_t CodeAlign, uintptr_t ROSize, uint32_t ROAlign,
      uintptr_t RWSize, uint32_t RWAlign) final;

  bool needsToReserveAllocationSpace() override { return true; }

  using llvm::SectionMemoryManager::EHFrameInfos;

  SingleSectionMemoryManager();

  void deregisterEHFrames() override;

  bool finalizeMemory(std::string* ErrMsg) override;

private:
  uintptr_t ImageBase = 0;
};
