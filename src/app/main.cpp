// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com


#if defined(_WIN32)
#if !defined(_MSC_VER)
#include <windows.h>
#include <mmsystem.h>
#include <ntdef.h>
extern "C" NTSYSAPI NTSTATUS NTAPI NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
#else
#include <Windows.h>
#include <mmsystem.h>
extern "C"  __declspec(dllimport) LONG __stdcall NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
#endif
#endif


#include "Application.hpp"
#include <ossia/detail/thread.hpp>
#include <score/widgets/MessageBox.hpp>

#include <clocale>

#include <QPixmapCache>
#include <QItemSelection>
#include <QSurfaceFormat>
#include <QApplication>
#include <qnamespace.h>

#ifndef QT_NO_OPENGL
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#endif
/*
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#include <QMessageBox>
#include <QTimer>
#endif
*/
#if defined(__linux__)
#include <dlfcn.h>
#if !_GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <link.h>
#include <sys/resource.h>
#define HAS_RLIMIT 1
#endif

#if defined(__linux__)
using X11ErrorHandler = int (*)(void*, void*);
using XSetErrorHandler_ptr = X11ErrorHandler (*)(X11ErrorHandler);

static struct {
  void* gdk_x11{};
  void* x11{};
  XSetErrorHandler_ptr x11_set_error_handler{};
} helper_dylibs;
#endif

#if defined(__SSE3__)
#include <pmmintrin.h>
#endif

#if defined(__APPLE__)
struct NSAutoreleasePool;

#include <sys/resource.h>

#define HAS_RLIMIT 1
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPreferences.h>
void disableAppRestore()
{
  CFPreferencesSetAppValue(
      CFSTR("NSQuitAlwaysKeepsWindows"), kCFBooleanFalse,
      kCFPreferencesCurrentApplication);

  CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

// Defined in mac_main.m
extern "C" void disableAppNap();
#endif

static void setup_x11()
{
#if defined(__linux__)
  helper_dylibs.x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
  if(helper_dylibs.x11)
  {
    helper_dylibs.x11_set_error_handler = reinterpret_cast<XSetErrorHandler_ptr>(dlsym(helper_dylibs.x11, "XSetErrorHandler"));
    assert(helper_dylibs.x11_set_error_handler);

    if(auto sym = reinterpret_cast<int(*)()>(dlsym(helper_dylibs.x11, "XInitThreads")))
    {
      if(!sym())
      {
        qDebug() << "Failed to initialise xlib thread support.";
      }
    }
  }
#endif
}

static void setup_suil()
{
#if defined(__linux__)
  enum SuilArg { SUIL_ARG_NONE };
  using suil_init_t = void (*)(int* argc, char*** argv, SuilArg key, ...);

  if(auto lib = dlopen("libsuil-0.so.0", RTLD_LAZY | RTLD_LOCAL))
  if(auto sym = reinterpret_cast<suil_init_t>(dlsym(lib, "suil_init")))
  {
    static int argc{0};
    static char** argv{nullptr};
    sym(&argc, &argv, SUIL_ARG_NONE);
  }
#endif
}

static void setup_gdk()
{
#if defined(__linux__)
  static bool gtk3_loaded{};
  dl_iterate_phdr([] (struct dl_phdr_info *info,
                  size_t size, void *data) -> int {
    if(std::string_view(info->dlpi_name).find(std::string_view("qgtk3")) != std::string_view::npos)
      gtk3_loaded = true;
    return 0;
  }, nullptr);

  if(gtk3_loaded)
    return;

  // Fun fact: this code has for lineage
  // WebKit (https://bugs.webkit.org/show_bug.cgi?id=44324) -> KDE -> maybe Netscape (?)
  using gdk_init_check_ptr = void *(*)(int*, char***);

  // For the handler thing: see
  // https://github.com/qt/qtbase/blob/dev/src/plugins/platformthemes/gtk3/qgtk3theme.cpp#L88
  // Basically GDK sets an error handler which exits... even though it's against the spec.
  X11ErrorHandler old_handler{};
  if(helper_dylibs.x11_set_error_handler)
    old_handler = helper_dylibs.x11_set_error_handler(nullptr);

  helper_dylibs.gdk_x11 = dlopen("libgdk-x11-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (helper_dylibs.gdk_x11)
  {
    if(auto gdk_init_check = (gdk_init_check_ptr)dlsym(helper_dylibs.gdk_x11, "gdk_init_check"))
      gdk_init_check(0, 0);
  }

  if(helper_dylibs.x11_set_error_handler)
    helper_dylibs.x11_set_error_handler(old_handler);

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
  if(!qEnvironmentVariableIsEmpty("FAUST_LIB_PATH"))
    return;

  auto path = ossia::get_exe_path();
  auto last_slash =
#if defined(_WIN32)
      path.find_last_of('\\');
#else
      path.find_last_of('/');
#endif
  if(last_slash == std::string::npos)
    return;

  path = path.substr(0, last_slash);

#if defined(SCORE_DEPLOYMENT_BUILD)
#if defined(__APPLE__)
  path += "/../Resources/Faust";
#elif defined(__linux__)
  path += "/../share/faust";
#elif defined(_WIN32)
  path += "/faust";
#endif
#else
  path += "/src/plugins/score-plugin-faust/faustlibs-prefix/src/faustlibs";
#endif

  qputenv("FAUST_LIB_PATH", path.c_str());
}

static void setup_opengl(bool& enable_opengl_ui)
{
  const auto& plat = qApp->platformName();
  if(plat == "minimal" || plat == "offscreen" || plat == "vnc")
  {
    return;
  }

#ifndef QT_NO_OPENGL
#if defined(__arm__)
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setSwapInterval(1);
  fmt.setMajorVersion(3);
  fmt.setMinorVersion(2);
  fmt.setDefaultFormat(fmt);
#else
  {
    std::vector<std::pair<int, int>> versions_to_test = {
        { 4, 6 }, { 4, 5 }, { 4, 4 }, { 4, 3 }, { 4, 2 }, { 4, 1 }, { 4, 0 },
        { 3, 3 }, { 3, 2 }, { 3, 1 }, { 3, 0 },
        { 2, 1 }, { 2, 0 }
    };

    QOffscreenSurface surf;
    surf.create();
    {
      QOpenGLContext ctx;
      if(!ctx.create())
      {
        qDebug() << "OpenGL detection skipped, cannot create any OpenGL context";
        enable_opengl_ui = false;
        return;
      }
      if(!ctx.makeCurrent(&surf))
      {
        qDebug() << "OpenGL detection skipped, cannot create make an offscreen surface current";
        enable_opengl_ui = false;
        return;
      }

      ctx.functions()->initializeOpenGLFunctions();

      if(auto gl_renderer = (const char*)ctx.functions()->glGetString(GL_RENDERER))
      {
        if(auto ver = QString::fromUtf8(gl_renderer); ver.contains("llvmpipe", Qt::CaseInsensitive))
        {
          qDebug() << "LLVMPIPE detected, not changing the default format as it crashes";
          enable_opengl_ui = false;
          return;
        }
      }
    }

    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1);
    bool ok = false;
    for(auto [maj, min] : versions_to_test) {
      fmt.setMajorVersion(maj);
      fmt.setMinorVersion(min);

      QOpenGLContext ctx;
      ctx.setFormat(fmt);
      if(ctx.create())
      {
        if(ctx.makeCurrent(&surf))
        {
          qDebug().nospace() << "Using highest available OpenGL version: " << ctx.format().majorVersion() << "." << ctx.format().minorVersion();
          if(maj < 3 || (maj == 3 && min < 2))
            qDebug() << "Warning ! This OpenGL version is too old for every feature to work correctly. Consider updating your graphics card.";

          if(maj < 2)
            // GL 1: we don't even try
            ok = false;
          else
            // GL 2: we try
            ok = true;
          break;
        }
      }
    }
    if(!ok)
    {
      qDebug() << "OpenGL minimum version not supported";
      enable_opengl_ui = false;
    }
    else
    {
      fmt.setDefaultFormat(fmt);
    }
  }
#endif
#endif
  return;
}

static void setup_locale()
{
  QLocale::setDefault(QLocale::C);
  setlocale(LC_ALL, "C");
}

static void setup_app_flags()
{
  // Consistency in looks across macOS, Windows (which prevents the horrible 125% scaling) and Linux
  qputenv("QT_FONT_DPI", "96");

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qputenv("QT_STYLE_OVERRIDE", "phantom");
#endif

  if(!qEnvironmentVariableIsSet("QT_SUBPIXEL_AA_TYPE"))
    qputenv("QT_SUBPIXEL_AA_TYPE", "RGB");

#if defined(__EMSCRIPTEN__)
  qRegisterMetaType<Qt::ApplicationState>();
  qRegisterMetaType<QItemSelection>();
  QCoreApplication::setAttribute(Qt::AA_ForceRasterWidgets, true);
#endif

#if !defined(__EMSCRIPTEN__)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
  QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, true);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  QCoreApplication::setAttribute(Qt::AA_DontShowShortcutsInContextMenus, false);
#endif
#if defined(__linux__)
  // Else things look horrible on KDE plasma, etc
  qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");

  // https://github.com/ossia/score/issues/953
  // https://github.com/ossia/score/issues/1046
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
#endif
}

#if FFTW3_HAS_THREADS
#include <fftw3.h>
static void setup_fftw()
{
  // See http://fftw.org/fftw3_doc/Thread-safety.html
  fftw_make_planner_thread_safe();
}
#else
static void setup_fftw() { }
#endif

static void setup_limits()
{
#if HAS_RLIMIT
  constexpr int min_fds = 10000;
  struct rlimit rlim;
  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0)
    return;

  if (rlim.rlim_cur != RLIM_INFINITY && rlim.rlim_cur < rlim_t(min_fds))
  {
    if (rlim.rlim_max == RLIM_INFINITY)
      rlim.rlim_cur = min_fds;
    else if (rlim.rlim_cur == rlim.rlim_max)
      return;
    else
      rlim.rlim_cur = rlim.rlim_max;

    setrlimit(RLIMIT_NOFILE, &rlim);
  }
#endif
}
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
namespace
{
struct failsafe {
  const bool fs = this->read();

  explicit failsafe()
  {
    if(!fs)
    {
      this->write();
    }
    else
    {
      fprintf(stderr, "ossia score: starting in failsafe mode\n");
      fflush(stderr);
    }
  }

  QString path()
  {
    auto conf = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return conf + QDir::separator() +
           "ossia" + QDir::separator() +
           "failsafe.bit";
  }

  bool read()
  {
    return QFile::exists(this->path());
  }

  void write()
  {
    QFile f{this->path()};
    if(f.open(QIODevice::WriteOnly))
      f.write("1");
  }

  void clear()
  {
    // We only clear the failsafe if it was not set otherwise
    // it would crash every other time..
    if(!fs)
    {
      QFile f{this->path()};
      f.remove();
    }
  }

  operator bool() const noexcept { return fs; }
};
}

int main(int argc, char** argv)
{
  struct failsafe failsafe;

#if defined(__APPLE__)
  disableAppRestore();
  disableAppNap();
  qputenv("QT_MAC_WANTS_LAYER", "1");
#endif

  setup_limits();
  setup_suil();
  setup_x11();
  disable_denormals();
  setup_faust_path();
  setup_locale();
  setup_app_flags();
  setup_fftw();

  QPixmapCache::setCacheLimit(819200);
  Application app(argc, argv);

  setup_gdk();

#if defined(__APPLE__)
  disableAppNap();
#endif

  if(failsafe)
    app.appSettings.opengl = false;

#if defined(__linux__)
  // On linux under offscreen, etc it crashes inside
  // QOffscreenSurface::create
  // so we check if we set --no-opengl explicitly
  if(app.appSettings.opengl)
#endif
    setup_opengl(app.appSettings.opengl);

  failsafe.clear();
  QTimer::singleShot(1, &app, &Application::init);

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
