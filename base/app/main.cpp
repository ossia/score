// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <QApplication>
#include <QPixmapCache>
#include <qnamespace.h>
#include <QItemSelectionModel>
#include <QSurfaceFormat>
#include <ossia/detail/thread.hpp>

#if defined(__linux__)
#  include <X11/Xlib.h>
#endif

#if defined(__SSE3__)
#  include <pmmintrin.h>
#endif

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

#if defined(SCORE_STATIC_QT)
#  if defined(__linux__)
#    include <QtPlugin>

Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#  endif
#endif


static void setup_x11()
{
#if defined(__linux__)
  if (!XInitThreads())
  {
    qDebug() << "Failed to initialise xlib thread support.";
  }
#endif
}

static void disable_denormals()
{
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
}

static void setup_faust_path()
{
#if defined(__APPLE__)
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

  // TODO windows
}

static void setup_opengl()
{
#if !defined(__EMSCRIPTEN__)
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setMajorVersion(4);
  fmt.setMinorVersion(1);
  fmt.setSamples(1);
  fmt.setDefaultFormat(fmt);
#else
  QSurfaceFormat fmt;
  fmt.setAlphaBufferSize(0);
  fmt.setDefaultFormat(fmt);
#endif
}

static void setup_locale()
{
  QLocale::setDefault(QLocale::C);
  setlocale(LC_ALL, "C");
}

static void setup_app_flags()
{
#if defined(__EMSCRIPTEN__)
  qRegisterMetaType<Qt::ApplicationState>();
  qRegisterMetaType<QItemSelection>();
  QCoreApplication::setAttribute(Qt::AA_ForceRasterWidgets, true);
#endif

#if !defined(__EMSCRIPTEN__)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents);
#endif
}


int main(int argc, char** argv)
{
#if defined(__APPLE__)
  auto pool = mac_init_pool();

  disableAppRestore();
  qputenv("QT_MAC_WANTS_LAYER", "1");
#endif

  setup_x11();
  disable_denormals();
  setup_faust_path();
  setup_locale();
  setup_opengl();
  setup_app_flags();

  QPixmapCache::setCacheLimit(819200);
  Application app(argc, argv);;
  app.init();
  int res = app.exec();

#if defined(__APPLE__)
  mac_finish_pool(pool);
#endif
  return res;
}

#if defined(Q_CC_MSVC)
#include <ShlObj.h>
#include <qt_windows.h>
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>
static inline char *wideToMulti(int codePage, const wchar_t *aw)
{
    const int required = WideCharToMultiByte(codePage, 0, aw, -1, NULL, 0, NULL, NULL);
    char *result = new char[required];
    WideCharToMultiByte(codePage, 0, aw, -1, result, required, NULL, NULL);
    return result;
}

extern "C" int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR /*cmdParamarg*/, int /* cmdShow */)
{
    int argc;
    wchar_t **argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW)
        return -1;
    char **argv = new char *[argc + 1];
    for (int i = 0; i < argc; ++i)
        argv[i] = wideToMulti(CP_ACP, argvW[i]);
    argv[argc] = nullptr;
    LocalFree(argvW);
    const int exitCode = main(argc, argv);
    for (int i = 0; i < argc && argv[i]; ++i)
        delete [] argv[i];
    delete [] argv;
    return exitCode;
}

#endif
