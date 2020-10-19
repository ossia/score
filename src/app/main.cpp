// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"
#include <ossia/detail/thread.hpp>
#include <score/widgets/MessageBox.hpp>

#include <QPixmapCache>
#include <QSurfaceFormat>
#include <qnamespace.h>
/*
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#include <QMessageBox>
#include <QTimer>
#endif
*/
#if defined(__linux__)
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#include <Windows.h>
#include <mmsystem.h>
#include <ntdef.h>
extern "C" NTSYSAPI NTSTATUS NTAPI NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
#endif

#if defined(__SSE3__)
#include <pmmintrin.h>
#endif

#if defined(__APPLE__)
struct NSAutoreleasePool;
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPreferences.h>
void disableAppRestore()
{
  CFPreferencesSetAppValue(
      CFSTR("NSQuitAlwaysKeepsWindows"), kCFBooleanFalse,
      kCFPreferencesCurrentApplication);

  CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}
#endif

static void setup_x11()
{
#if defined(__linux__)
  if(auto x11 = dlopen("libX11.so", RTLD_LAZY | RTLD_LOCAL))
  if(auto sym = reinterpret_cast<int(*)()>(dlsym(x11, "XInitThreads")))
  {
    if(!sym())
    {
      qDebug() << "Failed to initialise xlib thread support.";
    }
  }
#endif
}

struct increase_timer_precision
{
  increase_timer_precision()
  {
#if defined(_WIN32)
  // First try to set the 1ms period
  timeBeginPeriod(1);

  // Then maybe we can go a bit lower...
  ULONG currentRes{};
  NtSetTimerResolution(100, TRUE, &currentRes);
#endif
  }

  ~increase_timer_precision()
  {
#if defined(_WIN32)
    timeEndPeriod(1);
#endif
  }
};

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
  auto path = ossia::get_exe_path();
#if defined(__APPLE__)
  auto last_slash = path.find_last_of('/');
  path = path.substr(0, last_slash);
  path += "/../Resources/Faust";
#elif defined(__linux__)
  auto last_slash = path.find_last_of('/');
  path = path.substr(0, last_slash);
  path += "/../share/faust";
#elif defined(_WIN32)
  auto last_slash = path.find_last_of('\\');
  path = path.substr(0, last_slash);
  path += "/faust";
#endif

  qputenv("FAUST_LIB_PATH", path.c_str());
}

static void setup_opengl()
{
  {
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setSwapInterval(1);
  fmt.setDefaultFormat(fmt);
  }
  return;
/*
#if !defined(__EMSCRIPTEN__)
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setMajorVersion(4);
  fmt.setMinorVersion(1);
  fmt.setProfile(QSurfaceFormat::CoreProfile);

  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(0);
  fmt.setRedBufferSize(8);
  fmt.setGreenBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setAlphaBufferSize(8);

  fmt.setRenderableType(QSurfaceFormat::OpenGL);
  fmt.setSamples(0);
  fmt.setSwapInterval(1);
  fmt.setColorSpace(QSurfaceFormat::sRGBColorSpace);

  fmt.setDefaultFormat(fmt);
#else
  QSurfaceFormat fmt;
  fmt.setAlphaBufferSize(0);
  fmt.setDefaultFormat(fmt);
#endif
*/
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
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
  QCoreApplication::setAttribute(Qt::AA_Use96Dpi, true);
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
  QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, true);
#endif

  QCoreApplication::setAttribute(Qt::AA_DontShowShortcutsInContextMenus, false);
#if defined(__linux__)
  // Else things look horrible on KDE plasma, etc
  qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");

  // https://github.com/ossia/score/issues/953
  // https://github.com/ossia/score/issues/1046
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif
}

int main(int argc, char** argv)
{
#if defined(__APPLE__)
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
  Application app(argc, argv);

  app.init();

  increase_timer_precision timerRes;

#if __has_include(<valgrind/callgrind.h>)
  /*
  QTimer::singleShot(5000, [] {
    score::information(nullptr, "debug start", "debug start");
    CALLGRIND_START_INSTRUMENTATION;
    QTimer::singleShot(10000, [] {
      CALLGRIND_STOP_INSTRUMENTATION;
      CALLGRIND_DUMP_STATS;
      score::information(nullptr, "debug stop", "debug stop");
    });
  });*/
#endif
  int res = app.exec();

  return res;
}

#if defined(Q_CC_MSVC)
#include <qt_windows.h>

#include <ShlObj.h>
#include <shellapi.h>
#include <stdio.h>
#include <windows.h>
static inline char* wideToMulti(int codePage, const wchar_t* aw)
{
  const int required
      = WideCharToMultiByte(codePage, 0, aw, -1, NULL, 0, NULL, NULL);
  char* result = new char[required];
  WideCharToMultiByte(codePage, 0, aw, -1, result, required, NULL, NULL);
  return result;
}

extern "C" int APIENTRY
WinMain(HINSTANCE, HINSTANCE, LPSTR /*cmdParamarg*/, int /* cmdShow */)
{
  int argc;
  wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argvW)
    return -1;
  char** argv = new char*[argc + 1];
  for (int i = 0; i < argc; ++i)
    argv[i] = wideToMulti(CP_ACP, argvW[i]);
  argv[argc] = nullptr;
  LocalFree(argvW);
  const int exitCode = main(argc, argv);
  for (int i = 0; i < argc && argv[i]; ++i)
    delete[] argv[i];
  delete[] argv;
  return exitCode;
}

#endif
