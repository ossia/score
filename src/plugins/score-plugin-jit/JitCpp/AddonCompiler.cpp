#include <JitCpp/AddonCompiler.hpp>
#include <JitCpp/Compiler/Driver.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Jit::AddonCompiler)
#if defined(__linux__) && LLVM_VERSION_MAJOR <= 10
#include <score_plugin_jit_export.h>
SCORE_PLUGIN_JIT_EXPORT int atexit (void (*__func) (void)) __THROW {
  return 0;
}
#endif

#if defined(WIN32)
namespace llvm::orc
{
}
#endif
namespace Jit
{
AddonCompiler::AddonCompiler()
{
  connect(
      this,
      &AddonCompiler::submitJob,
      this,
      &AddonCompiler::on_job,
      Qt::QueuedConnection);
  //this->moveToThread(&m_thread);
  //m_thread.start();
}

AddonCompiler::~AddonCompiler()
{
  //m_thread.exit(0);
  //m_thread.wait();
}

void AddonCompiler::on_job(
    std::string id,
    std::string cpp,
    std::vector<std::string> flags,
    CompilerOptions opts)
{
  try
  {
    // TODO this is needed because if the jit_plugin instance is removed,
    // function calls to this plug-in will crash. We must detect when a plugin
    // is not necessary anymore and remove it.
    using compiler_t = Driver<score::Plugin_QtInterface*()>;

    static std::list<std::unique_ptr<compiler_t>> ctx;

    flags.push_back("-DSCORE_JIT_ID=" + id);

    qDebug() << "Creating compiler...";
    ctx.push_back(std::make_unique<compiler_t>("plugin_instance_" + id));

    qDebug() << "Calling compiler...";
    auto jitedFn = (*ctx.back())(cpp, flags, opts);

    if(!jitedFn)
    {
      qDebug() << "could not compile plug-in: no factory";
      return;
    }

    qDebug() << "Invoking instance...";
    auto instance = jitedFn();
    if (!instance)
    {
      qDebug() << "could not compile plug-in: no instance";
      return;
    }
    else
    {
      qDebug() << "Compiled ok !";
      jobCompleted(instance);
    }
  }
  catch (const std::runtime_error& e)
  {
    qDebug() << "could not compile plug-in: " << e.what();
  }
}

}
