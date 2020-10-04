#pragma once

#include <JitCpp/JitOptions.hpp>
#include <QThread>

#include <score_plugin_jit_export.h>
#include <verdigris>
#undef RESET
namespace score
{
class Plugin_QtInterface;
}

namespace Jit
{
//! Compiles jobs asynchronously
class AddonCompiler final : public QObject
{
  W_OBJECT(AddonCompiler)
public:
  AddonCompiler();

  ~AddonCompiler();
  void submitJob(
      const std::string& id,
      std::string cpp,
      std::vector<std::string> flags,
      CompilerOptions opts) W_SIGNAL(submitJob, id, cpp, flags, opts);
  void jobCompleted(score::Plugin_QtInterface* p) W_SIGNAL(jobCompleted, p);
  void on_job(std::string id, std::string cpp, std::vector<std::string> flags,
              CompilerOptions opts);

private:
  QThread m_thread;
};

using FactoryFunction = std::function<void()>;
using CustomCompiler = std::function<
    FactoryFunction(const std::string&, const std::vector<std::string>&)>;
}

Q_DECLARE_METATYPE(std::string)
W_REGISTER_ARGTYPE(std::string)
Q_DECLARE_METATYPE(std::vector<std::string>)
W_REGISTER_ARGTYPE(std::vector<std::string>)
Q_DECLARE_METATYPE(Jit::CompilerOptions)
W_REGISTER_ARGTYPE(Jit::CompilerOptions)
Q_DECLARE_METATYPE(score::Plugin_QtInterface*)
W_REGISTER_ARGTYPE(score::Plugin_QtInterface*)
