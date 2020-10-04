#pragma once
#undef RESET
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/FileSystem.h>

#include <chrono>
#include <iostream>
#include <string>

namespace Jit
{
struct Exception final : std::runtime_error
{
  using std::runtime_error::runtime_error;
  Exception(llvm::Error E) : std::runtime_error{"JIT error"}
  {
    llvm::handleAllErrors(std::move(E), [&](const llvm::ErrorInfoBase& EI) {
      m_err = EI.message();
    });
  }

  const char* what() const noexcept override { return m_err.c_str(); }

private:
  std::string m_err;
};

struct Timer
{
  std::chrono::high_resolution_clock::time_point t0;
  Timer() { t0 = decltype(t0)::clock::now(); }
  ~Timer()
  {
    auto t1 = decltype(t0)::clock::now();
    std::cerr << "Took time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
                     .count()
              << "\n";
  }
};

inline llvm::Expected<std::unique_ptr<llvm::Module>>
readModuleFromBitcodeFile(llvm::StringRef bc, llvm::LLVMContext& context)
{
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer
      = llvm::MemoryBuffer::getFile(bc);
  if (!buffer)
    return llvm::errorCodeToError(buffer.getError());

  return llvm::parseBitcodeFile(buffer.get()->getMemBufferRef(), context);
}

static inline std::string
replaceExtension(llvm::StringRef name, llvm::StringRef ext)
{
  return name.substr(0, name.find_last_of('.') + 1).str() + ext.str();
}

static inline llvm::Error
return_code_error(llvm::StringRef message, int returnCode)
{
  return llvm::make_error<llvm::StringError>(
      message, std::error_code(returnCode, std::system_category()));
}

static inline llvm::Expected<std::string>
saveSourceFile(const std::string& content)
{
  using llvm::sys::fs::createTemporaryFile;

  int fd;
  llvm::SmallString<128> name;
  if (auto ec = createTemporaryFile("score-addon-cpp", "cpp", fd, name))
    return llvm::errorCodeToError(ec);

  constexpr bool shouldClose = true;
  constexpr bool unbuffered = true;
  llvm::raw_fd_ostream os(fd, shouldClose, unbuffered);
  os << content;

  return name.str().str();
}

}
