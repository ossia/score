// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <QApplication>
#include <QPixmapCache>
#include <qnamespace.h>
#include <ossia/detail/thread.hpp>
#if defined(__APPLE__)
struct NSAutoreleasePool;
#  include <CoreFoundation/CFNumber.h>
#  include <CoreFoundation/CFPreferences.h>
extern "C" NSAutoreleasePool* mac_init_pool();
extern "C" void mac_finish_pool(NSAutoreleasePool* pool);
void disableAppRestore()
{
  CFPreferencesSetAppValue(
      CFSTR("NSQuitAlwaysKeepsWindows"), kCFBooleanFalse,
      kCFPreferencesCurrentApplication);

  CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

#endif

#if defined(SCORE_STATIC_PLUGINS)
#  include <score_static_plugins.hpp>
#endif

#if defined(SCORE_STATIC_QT)
#  if defined(__linux__)
#    include <QtPlugin>

Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#  endif
#endif

#if defined(__linux__)
#  include <X11/Xlib.h>
#endif

#include <QItemSelectionModel>
#include <QSurfaceFormat>

static void init_plugins()
{
// TODO generate this too
#if defined(SCORE_STATIC_PLUGINS)
  Q_INIT_RESOURCE(score);
  Q_INIT_RESOURCE(ScenarioResources);
  Q_INIT_RESOURCE(DeviceExplorer);
#  if defined(SCORE_PLUGIN_TEMPORALAUTOMATAS)
  Q_INIT_RESOURCE(TAResources);
#  endif
#endif
}
#if defined(__SSE3__)
#  include <pmmintrin.h>
#endif
int main(int argc, char** argv)
{
#if defined(__linux__)
  if (!XInitThreads())
  {
    qDebug() << "Failed to initialise xlib thread support.";
  }
#endif

#if defined(__APPLE__)
  auto pool = mac_init_pool();
  disableAppRestore();
  qputenv("QT_MAC_WANTS_LAYER", "1");
  auto path = ossia::get_exe_path();
  auto last_slash = path.find_last_of('/');
  path = path.substr(0, last_slash);
  path += "/../Frameworks/Faust";
  qputenv("FAUST_LIB_PATH", path.c_str());
#elif defined(__linux__)
  auto path = ossia::get_exe_path();
  auto last_slash = path.find_last_of('/');
  path = path.substr(0, last_slash);
  path += "/../share/faust";
  qputenv("FAUST_LIB_PATH", path.c_str());
#endif

#if defined(__SSE3__)
  // See https://en.wikipedia.org/wiki/Denormal_number
  // and
  // http://stackoverflow.com/questions/9314534/why-does-changing-0-1f-to-0-slow-down-performance-by-10x
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#elif defined(__arm__)
  int x;
  asm("vmrs %[result],FPSCR \r\n"
      "bic %[result],%[result],#16777216 \r\n"
      "vmsr FPSCR,%[result]"
      : [result] "=r"(x)
      :
      :);
  printf("ARM FPSCR: %08x\n", x);
#endif

#if defined(__EMSCRIPTEN__)
  qRegisterMetaType<Qt::ApplicationState>();
  qRegisterMetaType<QItemSelection>();
#endif

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents);

  QLocale::setDefault(QLocale::C);
  setlocale(LC_ALL, "C");

  init_plugins();

  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setMajorVersion(4);
  fmt.setMinorVersion(1);
  fmt.setSamples(1);
  fmt.setDefaultFormat(fmt);

  QPixmapCache::setCacheLimit(819200);
  Application app(argc, argv);
  app.init();
  int res = app.exec();
#if defined(__APPLE__)
  mac_finish_pool(pool);
#endif
  return res;
}
