#pragma once
#include <JitCpp/JitPlatform.hpp>
#include <JitCpp/JitUtils.hpp>
#include <clang/Frontend/TextDiagnosticBuffer.h>

#include <string>
#include <vector>

namespace Jit
{
class ClangCC1Driver
{
public:
  ClangCC1Driver() = default;
  ~ClangCC1Driver();

  static std::optional<QDir> bitcodeDatabase();

  llvm::Expected<std::unique_ptr<llvm::Module>> compileTranslationUnit(
      const std::string& cppCode,
      const std::vector<std::string>& flags,
      CompilerOptions opts,
      llvm::LLVMContext& context);

private:
  //! Default compiler arguments
  static std::vector<std::string> getClangCC1Args(CompilerOptions opts);

  //! Actual invocation of clang
  static llvm::Error
  compileCppToBitcodeFile(const std::vector<std::string>& args);

  std::vector<std::function<void()>> m_deleters;
};

}
