// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

// clang-format off
#if defined(_WIN32)
#if !defined(_MSC_VER)
#include <ntdef.h>
#include <windows.h>
#include <mmsystem.h>
extern "C" NTSYSAPI NTSTATUS NTAPI NtSetTimerResolution(
    ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
#else
#include <Windows.h>
#include <mmsystem.h>
extern "C" __declspec(dllimport) LONG __stdcall NtSetTimerResolution(
    ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);

extern "C" int gettimeofday(struct timeval* tv, struct timezone* tz)
{
  return 0;
}
extern "C" void ___chkstk_ms(void) { }

#include <cmath>
extern "C" void sincos(double x, double* sin, double* cos)
{
  *sin = std::sin(x);
  *cos = std::cos(x);
}
#endif

#include <exception>
#include <winrt/Windows.Foundation.h>
#endif
// clang-format on

#include "Application.hpp"

#include <score/widgets/MessageBox.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/disable_fpe.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDir>
#include <QItemSelection>
#include <QPixmapCache>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTimer>
#include <qnamespace.h>

#include <clocale>

#ifndef QT_NO_OPENGL
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#endif
/*
#if __has_include(<valgrind/callgrind.h>)
#include <QMessageBox>
#include <QTimer>

#include <valgrind/callgrind.h>
#endif
*/
#if defined(__linux__)
#include <dlfcn.h>
#if !_GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <sys/resource.h>

#include <link.h>
#define HAS_RLIMIT 1
#endif

#if __has_include(<linux/kd.h>)
#include <linux/kd.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(__linux__)
using X11ErrorHandler = int (*)(void*, void*);
using XSetErrorHandler_ptr = X11ErrorHandler (*)(X11ErrorHandler);

static struct
{
  bool run_under_x11{};
  bool xwayland{};
  void* gtk2{};
  void* gtk3{};
  void* gtk4{};
  void* gdk_x11{};
  void* x11{};
  XSetErrorHandler_ptr x11_set_error_handler{};
} helper_dylibs;
#endif

#if defined(__SSE3__)
#include <pmmintrin.h>
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
#define __SANITIZE_ADDRESS__ 1
#endif
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

void ensureDyldPath()
{
  auto e = qEnvironmentVariable("DYLD_LIBRARY_PATH");
  if(e.isEmpty() || e == "/usr/lib/system/introspection")
  {
    return;
  }
  else
  {
    fprintf(
        stderr,
        "Fatal error: DYLD_LIBRARY_PATH must not be set on macOS.\n"
        "See https://github.com/ossia/score/issues/1711\n\n"
        "If using Qt Creator, simply go in Projects and\n"
        "uncheck the 'Add library search path to DYLD_LIBRARY_PATH and "
        "DYLD_FRAMEWORK_PATH'\n"
        "checkbox on the execution tab.\n\n"
        "Current DYLD_LIBRARY_PATH: '%s'\n",
        e.toStdString().c_str());

    // If you see this comment, you are allowed to remove it on your dev
    // build but be aware that you will likely hit very spurious random
    // crashes, for instance when a PNG gets loaded or when opening a file
    // dialog
    exit(1);
  }
}

// Defined in mac_main.m
extern "C" void disableAppNap();
#endif

extern "C" {
Q_DECL_EXPORT short NvOptimusEnablement = 1;
Q_DECL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;
}

static void setup_gpu()
{
#if defined(__linux__)
  if(QDir{"/proc/driver/nvidia/gpus"}
         .entryList(QDir::Dirs | QDir::NoDotAndDotDot)
         .count()
     > 0)
  {
    if(!qEnvironmentVariableIsSet("__GLX_VENDOR_LIBRARY_NAME"))
      qputenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia");
    if(!qEnvironmentVariableIsSet("__NV_PRIME_RENDER_OFFLOAD"))
      qputenv("__NV_PRIME_RENDER_OFFLOAD", "1");
  }
#endif
}

static void setup_x11(int argc, char** argv)
{
#if defined(__linux__)
  static const bool x11 = !qEnvironmentVariableIsEmpty("DISPLAY");
  static const bool wayland = !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
  const auto platform = qgetenv("QT_QPA_PLATFORM");
  const auto platform_in_args = std::any_of(argv, argv + argc, [](std::string_view str) {
    return str == "-platform" || str == "--platform";
  });
  const bool has_platform = !platform.isEmpty() || platform_in_args;

  if(!x11 && !wayland)
  {
    // Try eglfs
    qputenv("QT_QPA_PLATFORM", "eglfs");
    return;
  }
  static constexpr auto setup_x11_error_handling = [] {
    helper_dylibs.x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if(helper_dylibs.x11)
    {
      helper_dylibs.x11_set_error_handler = reinterpret_cast<XSetErrorHandler_ptr>(
          dlsym(helper_dylibs.x11, "XSetErrorHandler"));
      assert(helper_dylibs.x11_set_error_handler);

      if(auto sym
         = reinterpret_cast<int (*)()>(dlsym(helper_dylibs.x11, "XInitThreads")))
      {
        if(!sym())
        {
          qDebug() << "Failed to initialise xlib thread support.";
        }

        helper_dylibs.run_under_x11 = true;
        helper_dylibs.xwayland = wayland;
      }
    }
  };

#if QT_VERSION <= QT_VERSION_CHECK(7, 0, 0)
  // Wayland as of Qt 6 does not seem to support QRhi properly especially on nvidia
  // so we still force xcb
  if(!has_platform)
  {
    qputenv("QT_QPA_PLATFORM", "xcb");
    setup_x11_error_handling();
  };
#else
  // Only setup X11 stuff if we are going to use XCB for sure
  // or if we want to force running under xwayland:
  if(x11)
  {
    if(!wayland || platform == "xcb")
      setup_x11_error_handling();
  }
#endif
#endif
}

#if defined(__linux__)
extern "C" {
enum SuilArg
{
  SUIL_ARG_NONE
};
using suil_init_t = void (*)(int* argc, char*** argv, SuilArg key, ...);
}
#endif
static void setup_suil()
{
#if defined(__linux__)
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_AUDIOPLUGINS"))
    return;
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_LV2"))
    return;
  if(!helper_dylibs.run_under_x11)
    return;

  auto path = ossia::get_exe_folder();
  if(path.ends_with("/bin"))
    path.resize(path.size() - 4);

  // 1. In the appimage case we have to tell suil that libsuil_x11_in_qt6.so is
  // nrelative to the executable
  if(qEnvironmentVariableIsEmpty("SUIL_MODULE_DIR") && !path.empty())
  {
    auto suil_module_dir = path + "/lib/suil-0";
    if(QDir{}.exists(QString::fromStdString(suil_module_dir)))
    {
      qputenv("SUIL_MODULE_DIR", suil_module_dir.c_str());
    }
  }

  // 2. Init suil if necessary. We need to do this before QApplication
  // and thus before loading libscore_plugin_lv2.so which links to the right
  // libsuil-0.so...
  auto init_suil = [](void* self) {
    if(auto sym = reinterpret_cast<suil_init_t>(dlsym(self, "suil_init")))
    {
      static int argc{0};
      static char** argv{nullptr};
      sym(&argc, &argv, SUIL_ARG_NONE);
      return true;
    }
    return false;
  };

  if(auto self = dlopen(nullptr, RTLD_LAZY | RTLD_LOCAL))
  {
    if(init_suil(self))
      return;
  }

  if(QDir{}.exists(
         QString::fromStdString(path + "/src/plugins/score-plugin-lv2/libsuil-0.so.0")))
  {
    if(auto lib = dlopen(
           (path + "/src/plugins/score-plugin-lv2/libsuil-0.so.0").c_str(),
           RTLD_LAZY | RTLD_LOCAL))
    {
      if(init_suil(lib))
        return;
    }
  }

  if(auto lib = dlopen("libsuil-0.so.0", RTLD_LAZY | RTLD_LOCAL))
  {
    if(init_suil(lib))
      return;
  }
#endif
}

static void setup_gtk()
{
#if defined(__linux__)
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_AUDIOPLUGINS"))
    return;
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_LV2"))
    return;
  if(!helper_dylibs.run_under_x11)
    return;

  helper_dylibs.gtk2 = dlopen("libgtk-x11-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);
  if(helper_dylibs.gtk2)
  {
    if(auto sym = reinterpret_cast<void (*)(void)>(
           dlsym(helper_dylibs.gtk2, "gtk_disable_setlocale")))
      sym();
  }
  helper_dylibs.gtk3 = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
  if(helper_dylibs.gtk3)
  {
    if(auto sym = reinterpret_cast<void (*)(void)>(
           dlsym(helper_dylibs.gtk3, "gtk_disable_setlocale")))
      sym();
  }
  helper_dylibs.gtk4 = dlopen("libgtk-4.so.1", RTLD_LAZY | RTLD_LOCAL);
  if(helper_dylibs.gtk4)
  {
    if(auto sym = reinterpret_cast<void (*)(void)>(
           dlsym(helper_dylibs.gtk4, "gtk_disable_setlocale")))
      sym();
  }
#endif
}

static void setup_gdk()
{
#if defined(__linux__)
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_AUDIOPLUGINS"))
    return;
  if(qEnvironmentVariableIsSet("SCORE_DISABLE_LV2"))
    return;
  if(!helper_dylibs.run_under_x11)
    return;

  static bool gtk3_loaded{};
  dl_iterate_phdr(
      [](struct dl_phdr_info* info, size_t size, void* data) -> int {
        if(std::string_view(info->dlpi_name).find(std::string_view("qgtk3"))
           != std::string_view::npos)
          gtk3_loaded = true;
        return 0;
      },
      nullptr);

  if(gtk3_loaded)
    return;

  // Fun fact: this code has for lineage
  // WebKit (https://bugs.webkit.org/show_bug.cgi?id=44324) -> KDE -> maybe Netscape (?)
  using gdk_init_check_ptr = void* (*)(int*, char***);

  // For the handler thing: see
  // https://github.com/qt/qtbase/blob/dev/src/plugins/platformthemes/gtk3/qgtk3theme.cpp#L88
  // Basically GDK sets an error handler which exits... even though it's against the spec.
  X11ErrorHandler old_handler{};
  if(helper_dylibs.x11_set_error_handler)
    old_handler = helper_dylibs.x11_set_error_handler(nullptr);

  helper_dylibs.gdk_x11 = dlopen("libgdk-x11-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);
  if(helper_dylibs.gdk_x11)
  {
    if(auto gdk_init_check
       = (gdk_init_check_ptr)dlsym(helper_dylibs.gdk_x11, "gdk_init_check"))
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

static void setup_faust_path()
{
  if(!qEnvironmentVariableIsEmpty("FAUST_LIB_PATH"))
    return;

  auto path = ossia::get_exe_folder();
  if(path.empty())
    return;

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

  if(QFileInfo{QString::fromUtf8(path)}.isDir())
    qputenv("FAUST_LIB_PATH", path.c_str());
}

static void setup_opengl(bool& enable_opengl_ui)
{
  const auto& plat = qGuiApp->platformName();
  if(plat == "minimal" || plat == "offscreen" || plat == "vnc" || plat == "wasm")
  {
    return;
  }
  if(qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"))
    return;

#ifndef QT_NO_OPENGL
#if (defined(__arm__) || defined(__aarch64__)) && !defined(_WIN32) && !defined(__APPLE__)
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setRenderableType(QSurfaceFormat::OpenGLES);
  fmt.setSwapInterval(1);
  fmt.setMajorVersion(3);
  fmt.setMinorVersion(2);
  fmt.setDefaultFormat(fmt);
#elif defined(__APPLE__)
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setSwapInterval(1);
  fmt.setMajorVersion(4);
  fmt.setMinorVersion(1);
  fmt.setDefaultFormat(fmt);
#else
  {
    std::vector<std::pair<int, int>> versions_to_test
        = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
           {3, 3}, {3, 2}, {3, 1}, {3, 0}, {2, 1}, {2, 0}};

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
        qDebug() << "OpenGL detection skipped, cannot make an offscreen surface "
                    "current";
        enable_opengl_ui = false;
        return;
      }

      ctx.functions()->initializeOpenGLFunctions();

      if(auto gl_renderer = (const char*)ctx.functions()->glGetString(GL_RENDERER))
      {
        if(auto ver = QString::fromUtf8(gl_renderer);
           ver.contains("llvmpipe", Qt::CaseInsensitive))
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
    for(auto [maj, min] : versions_to_test)
    {
      fmt.setMajorVersion(maj);
      fmt.setMinorVersion(min);

      QOpenGLContext ctx;
      ctx.setFormat(fmt);
      if(ctx.create())
      {
        if(ctx.makeCurrent(&surf))
        {
          qDebug().nospace() << "Using highest available OpenGL version: "
                             << ctx.format().majorVersion() << "."
                             << ctx.format().minorVersion();
          if(maj < 3 || (maj == 3 && min < 2))
            qDebug() << "Warning ! This OpenGL version is too old for every feature to "
                        "work correctly. Consider updating your graphics card.";

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
  qputenv("QSG_USE_SIMPLE_ANIMATION_DRIVER", "1");
  qputenv("QSG_RENDER_LOOP", "basic");
  // Consistency in looks across macOS, Windows (which prevents the horrible 125% scaling) and Linux
  // FIXME in Qt 6 this is entirely broken... https://bugreports.qt.io/browse/QTBUG-103225
  // qputenv("QT_FONT_DPI", "96");

  if(!qEnvironmentVariableIsSet("QT_SUBPIXEL_AA_TYPE"))
    qputenv("QT_SUBPIXEL_AA_TYPE", "RGB");

  // Really makes monospace HintSlight fonts look better
  if(!qEnvironmentVariableIsSet("FREETYPE_PROPERTIES"))
    qputenv("FREETYPE_PROPERTIES", "cff:no-stem-darkening=0 autofitter:no-stem-darkening=0 type1:no-stem-darkening=0 t1cid:no-stem-darkening=0");

#if defined(__EMSCRIPTEN__)
  qRegisterMetaType<Qt::ApplicationState>();
  qRegisterMetaType<QItemSelection>();
  QCoreApplication::setAttribute(Qt::AA_ForceRasterWidgets, true);
#endif

#if !defined(__EMSCRIPTEN__)
  QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, true);
#endif

  QCoreApplication::setAttribute(Qt::AA_DontShowShortcutsInContextMenus, false);
#if defined(__linux__)
  // Else things look horrible on KDE plasma, etc
  qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
  // qputenv("QT_NO_GLIB", "1"); // looks like it actually prevents the app from starting on GNOME

  if(!qEnvironmentVariableIsSet("FLATPAK_ID"))
  {
    // https://github.com/ossia/score/issues/953
    // https://github.com/ossia/score/issues/1046
    // https://github.com/ossia/score/issues/1725 for the flatpak reenablement
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
  }
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
#endif
}

#if FFTW_HAS_THREADS
#include <fftw3.h>
static void setup_fftw()
{
  // See http://fftw.org/fftw3_doc/Thread-safety.html
#if defined(OSSIA_FFTW_SINGLE_ONLY)
  fftwf_make_planner_thread_safe();
  fftwf_plan_with_nthreads(1);
  fftwf_init_threads();
#else
  fftw_make_planner_thread_safe();
  fftw_plan_with_nthreads(1);
  fftw_init_threads();
#endif
}
#else
static void setup_fftw() { }
#endif

// tty functions needed because when exiting with
// eglfs and vkkhrdisplay we cannot use the keyboard anymore
// https://stackoverflow.com/a/66526943
#if defined(KDSKBMODE)
static int linux_tty_k_mode = -1;
static void read_linux_tty()
{
  int tfd = open("/dev/tty", O_RDWR);
  if(tfd == -1)
    return;
  ioctl(tfd, KDGKBMODE, &linux_tty_k_mode);
  close(tfd);
}
static void restore_linux_tty()
{
  int tfd = open("/dev/tty", O_RDWR);
  if(tfd == -1)
    return;
  ioctl(tfd, KDSETMODE, KD_TEXT);          // return to text mode from graphics mode
  ioctl(tfd, 0x4b51, 0);                   // 0x4b51=KDKBMUTE, from qfbvthandler.cpp
  ioctl(tfd, KDSKBMODE, linux_tty_k_mode); // set keyboard mode back
  linux_tty_k_mode = -1;
  const auto blink_on = "\033[9;15]\033[?33h\033[?25h\033[?0c"; // from qfbvthandler.cpp
  write(tfd, blink_on, strlen(blink_on) + 1); // enable blanking and cursor
  close(tfd);
}
#endif

static void setup_priority()
{
#if defined(_WIN32)
  SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#endif
}

static void setup_limits()
{
#if HAS_RLIMIT
  constexpr int min_fds = 10000;
  struct rlimit rlim;
  if(getrlimit(RLIMIT_NOFILE, &rlim) != 0)
    return;

  if(rlim.rlim_cur != RLIM_INFINITY && rlim.rlim_cur < rlim_t(min_fds))
  {
    if(rlim.rlim_max == RLIM_INFINITY)
      rlim.rlim_cur = min_fds;
    else if(rlim.rlim_cur == rlim.rlim_max)
      return;
    else
      rlim.rlim_cur = rlim.rlim_max;

    setrlimit(RLIMIT_NOFILE, &rlim);
  }
#endif
}

namespace
{
static void setup_qml()
{
#if defined(__SANITIZE_ADDRESS__)
  if(!qEnvironmentVariableIsSet("QV4_FORCE_INTERPRETER"))
    qputenv("QV4_FORCE_INTERPRETER", "1");
  if(!qEnvironmentVariableIsSet("QV4_MM_AGGRESSIVE_GC"))
    qputenv("QV4_MM_AGGRESSIVE_GC", "1");
#endif
  if(!qEnvironmentVariableIsSet("QV4_JS_MAX_STACK_SIZE"))
    qputenv("QV4_JS_MAX_STACK_SIZE", "16777216");
  if(!qEnvironmentVariableIsSet("QV4_GC_MAX_STACK_SIZE"))
    qputenv("QV4_GC_MAX_STACK_SIZE", "16777216");
}

struct failsafe
{
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
    return conf + QDir::separator() + "ossia" + QDir::separator() + "failsafe.bit";
  }

  bool read() { return QFile::exists(this->path()); }

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
#if defined(_WIN32)
  winrt::uninit_apartment();
  winrt::init_apartment(winrt::apartment_type::single_threaded);
#endif

  struct failsafe failsafe;

#if defined(__APPLE__)
  disableAppRestore();
  disableAppNap();
  ensureDyldPath();
  // FIXME broken in qt6 qputenv("QT_MAC_WANTS_LAYER", "1");
#endif

#if defined(KDSKBMODE)
  read_linux_tty();
#endif

  setup_priority();
  setup_limits();
  setup_gpu();
  setup_x11(argc, argv);
  setup_gtk();
  setup_suil();
  setup_faust_path();
  setup_locale();
  setup_app_flags();
  setup_fftw();
  setup_qml();

  QPixmapCache::setCacheLimit(819200);
  Application app(argc, argv);

#if defined(_WIN32)
  winrt::uninit_apartment();
  winrt::init_apartment(winrt::apartment_type::single_threaded);
#endif

#if defined(KDSKBMODE)
  {
    const auto& plat = QGuiApplication::platformName();
    const bool linux_tty_needs_restore = plat == "eglfs" || plat == "vkkhrdisplay";
    if(linux_tty_needs_restore)
      qAddPostRoutine([] { restore_linux_tty(); });
  }
#endif

  setup_gdk();

#if defined(__APPLE__)
  disableAppNap();
#endif

#if defined(__EMSCRIPTEN__)
  app.appSettings.opengl = false;
#endif

  if(failsafe)
    app.appSettings.opengl = false;

#if defined(__linux__) && !(defined(__arm__) || defined(__aarch64__))
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

  ossia::disable_fpe();
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
  const int required = WideCharToMultiByte(codePage, 0, aw, -1, NULL, 0, NULL, NULL);
  char* result = new char[required];
  WideCharToMultiByte(codePage, 0, aw, -1, result, required, NULL, NULL);
  return result;
}

extern "C" int APIENTRY
WinMain(HINSTANCE, HINSTANCE, LPSTR /*cmdParamarg*/, int /* cmdShow */)
{
  int argc;
  wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if(!argvW)
    return -1;
  char** argv = new char*[argc + 1];
  for(int i = 0; i < argc; ++i)
    argv[i] = wideToMulti(CP_ACP, argvW[i]);
  argv[argc] = nullptr;
  LocalFree(argvW);
  const int exitCode = main(argc, argv);
  for(int i = 0; i < argc && argv[i]; ++i)
    delete[] argv[i];
  delete[] argv;
  return exitCode;
}

#endif
