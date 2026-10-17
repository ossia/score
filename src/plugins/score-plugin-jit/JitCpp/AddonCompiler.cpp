#include <JitCpp/AddonCompiler.hpp>
#include <JitCpp/Compiler/Driver.hpp>

#include <wobjectimpl.h>

#include <string_view>

W_OBJECT_IMPL(Jit::AddonCompiler)

namespace Jit
{
static constexpr std::string_view qt_metatype_instantiations = R"_(
#if !defined(_WIN32)
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qbitarray.h>
#include <QtCore/qcborarray.h>
#include <QtCore/qcbormap.h>
#include <QtCore/qcborvalue.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qfloat16.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qline.h>
#include <QtCore/qlocale.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsize.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE
namespace QtPrivate
{
#define SCORE_JIT_DEFINE_METATYPE(TypeName, Id, Name) \
  template class QMetaTypeForType<Name>;              \
  template struct QMetaTypeInterfaceWrapper<Name>;

QT_FOR_EACH_STATIC_PRIMITIVE_NON_VOID_TYPE(SCORE_JIT_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_PRIMITIVE_POINTER(SCORE_JIT_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_CORE_CLASS(SCORE_JIT_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_CORE_POINTER(SCORE_JIT_DEFINE_METATYPE)
QT_FOR_EACH_STATIC_CORE_TEMPLATE(SCORE_JIT_DEFINE_METATYPE)

#undef SCORE_JIT_DEFINE_METATYPE
}
QT_END_NAMESPACE
#endif
)_";

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
    cpp.append(qt_metatype_instantiations);

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
