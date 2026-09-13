#include <JitCpp/AddonCompiler.hpp>
#include <JitCpp/Compiler/Driver.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Jit::AddonCompiler)

namespace Jit
{
AddonCompiler::AddonCompiler()
{
  connect(
      this, &AddonCompiler::submitJob, this, &AddonCompiler::on_job,
      Qt::QueuedConnection);
  //this->moveToThread(&m_thread);
  //m_thread.start();
}

AddonCompiler::~AddonCompiler()
{
  //m_thread.exit(0);
  //m_thread.wait();

  // Abandoned, not closed: endSession() hands the linked graphs back to the memory
  // manager, and the deallocation actions it runs on the way out are calls into
  // the add-on's own JIT-mapped memory, which faults on macOS/arm64. The add-on is
  // still registered with the application here in any case.
  for(auto& compiler : m_compilers)
    (void)compiler.release();
}

void AddonCompiler::on_job(
    std::string id, std::string cpp, std::vector<std::string> flags,
    CompilerOptions opts)
{
  try
  {
    // TODO this is needed because if the jit_plugin instance is removed,
    // function calls to this plug-in will crash. We must detect when a plugin
    // is not necessary anymore and remove it.
    using compiler_t = Driver;

    flags.push_back("-DSCORE_JIT_ID=" + id);

    qDebug() << "Creating compiler...";
    m_compilers.push_back(std::make_unique<compiler_t>("plugin_instance_" + id));

    qDebug() << "Calling compiler...";
    auto jitedFn = (*m_compilers.back())
                       .operator()<score::Plugin_QtInterface*()>(cpp, flags, opts);

    if(!jitedFn)
    {
      qDebug() << "could not compile plug-in: no factory";
      jobFailed();
      return;
    }

    qDebug() << "Invoking instance...";
    auto instance = jitedFn();
    if(!instance)
    {
      qDebug() << "could not compile plug-in: no instance";
      jobFailed();
      return;
    }
    else
    {
      qDebug() << "Compiled ok !";
      jobCompleted(instance);
    }
  }
  catch(const std::runtime_error& e)
  {
    qDebug() << "could not compile plug-in: " << e.what();
    jobFailed();
  }
}

}
