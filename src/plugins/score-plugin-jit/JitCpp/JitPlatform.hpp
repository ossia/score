#pragma once
#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>

#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>

#if LLVM_VERSION_MAJOR >= 17
#include <llvm/TargetParser/Host.h>
#else
#include <llvm/Support/Host.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <version>
#if __has_include(<llvm/Config/llvm-config-64.h>)
#include <llvm/Config/llvm-config-64.h>
#elif __has_include(<llvm/Config/llvm-config.h>)
#include <llvm/Config/llvm-config.h>
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
#define __SANITIZE_ADDRESS__ 1
#endif
#endif

#if defined(SCORE_FHS_BUILD)
#define SCORE_USE_DISTRO_SYSROOT 1
#else
#if defined(SCORE_DEPLOYMENT_BUILD)
#define SCORE_USE_DISTRO_SYSROOT 0
#else
#define SCORE_USE_DISTRO_SYSROOT 1
#endif
#endif

#include <JitCpp/JitOptions.hpp>

#include <score_git_info.hpp>

namespace Jit
{

static inline std::string locateSDK()
{
  if(QString sdk = qgetenv("SCORE_JIT_SDK"); !sdk.isEmpty())
    return sdk.toStdString();

  auto& ctx = score::AppContext().settings<Library::Settings::Model>();
  QString path = ctx.getSDKPath();

  if(QString libPath = QStringLiteral("%1/%2/usr").arg(path).arg(SCORE_TAG_NO_V);
     QDir(libPath + "/include/c++").exists())
  {
    return libPath.toStdString();
  }

  if(QString libPath = path + "/usr"; QDir(libPath + "/include/c++").exists())
  {
    return libPath.toStdString();
  }

  auto appFolder = qApp->applicationDirPath();

#if !SCORE_FHS_BUILD

#if defined(_WIN32)
  {
    QDir d{appFolder};
    d.cd("sdk");
    return d.absolutePath().toStdString();
  }
#elif defined(__linux__)
  {
    QDir d{appFolder};
    d.cdUp();
    d.cd("usr");
    return d.absolutePath().toStdString();
  }
#elif defined(__APPLE__)
  {
    QDir d{appFolder};
    d.cdUp();
    if(d.cd("Frameworks"))
    {
      if(d.cd("Score.Framework"))
      {
        return d.absolutePath().toStdString();
      }
    }
    auto framework = QString(appFolder + "/Score.Framework");
    if(QDir{}.exists(framework))
      return framework.toStdString();
    else
      return "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
             "Developer/SDKs/MacOSX.sdk/usr";
  }
#endif

#else
#if defined(__APPLE__)
  return "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/"
         "Developer/SDKs/MacOSX.sdk/usr";
#endif
  if(QFileInfo("/usr/include/c++").isDir())
  {
    return "/usr";
  }
  else
  {
    return ctx.getSDKPath().toStdString();
  }
#endif
}

static inline void
populateCompileOptions(std::vector<std::string>& args, CompilerOptions opts)
{
  args.push_back("-triple");
  args.push_back(llvm::sys::getProcessTriple());

  args.push_back("-target-cpu");
  args.push_back(llvm::sys::getHostCPUName().lower());

  {
    llvm::StringMap<bool> HostFeatures;
#if LLVM_VERSION_MAJOR < 19
    bool ok = llvm::sys::getHostCPUFeatures(HostFeatures);
#else
    constexpr bool ok = true;
    HostFeatures = llvm::sys::getHostCPUFeatures();
#endif
    if(ok)
    {
      for(const llvm::StringMapEntry<bool>& F : HostFeatures)
      {
        args.push_back("-target-feature");
        args.push_back((F.second ? "+" : "-") + F.first().str());
      }
    }
  }

  args.push_back("-std=c++23");
  args.push_back("-disable-free");
  args.push_back("-fdeprecated-macro");
  args.push_back("-fmath-errno");
  // disappeared in clang 11 args.push_back("-fuse-init-array");

  // args.push_back("-mrelocation-model");
  // args.push_back("static");
  args.push_back("-mthread-model");
  args.push_back("posix");
  // disappeared in clang 11 args.push_back("-masm-verbose");
  args.push_back("-mconstructor-aliases");

  // args.push_back("-dwarf-column-info");
  // args.push_back("-debugger-tuning=gdb");

  args.push_back("-fno-use-cxa-atexit");

  // -Ofast stuff:
  // args.push_back("-menable-unsafe-fp-math");
  args.push_back("-fno-signed-zeros");
  args.push_back("-mreassociate");
  args.push_back("-freciprocal-math");
  args.push_back("-fno-rounding-math");

  // disappeared in clang 12 args.push_back("-fno-trapping-math");
  args.push_back("-ffp-contract=fast");

#if !defined(__linux__) // || (defined(__linux__) && __GLIBC_MINOR__ >= 31)
  // isn't that great
  // https://reviews.llvm.org/D74712
  args.push_back("-Ofast");
  args.push_back("-menable-no-infs");
  args.push_back("-menable-no-nans");
  args.push_back("-ffinite-math-only");
  args.push_back("-ffast-math");
#else
  args.push_back("-O3");
  args.push_back("-fno-builtin");
#endif

  // Prevent emitting ___chkstk_ms calls in alloca
  args.push_back("-mno-stack-arg-probe");

  args.push_back("-fgnuc-version=4.2.1");

#if defined(__APPLE__)
  args.push_back("-fmax-type-align=16");
#endif
  args.push_back("-mrelocation-model");
  args.push_back("pic");
  args.push_back("-pic-level");
  args.push_back("2");
  //args.push_back("-pic-is-pie");

  // changed from -fvisibility hidden to -fvisibility=hidden in clang 16
  args.push_back("-fvisibility=hidden");

  args.push_back("-fvisibility-inlines-hidden");

  // // tls:
  args.push_back("-ftls-model=local-exec");
  args.push_back("-femulated-tls");

  // if fsanitize:
  args.push_back("-mrelax-all");
  args.push_back("-disable-llvm-verifier");
  args.push_back("-discard-value-names");
#if defined(__SANITIZE_ADDRESS__)
  /*

  args.push_back(
      "-fsanitize=address,alignment,array-bounds,bool,builtin,enum,float-cast-"
      "overflow,float-divide-by-zero,function,integer-divide-by-zero,nonnull-"
      "attribute,null,pointer-overflow,return,returns-nonnull-attribute,shift-"
      "base,shift-exponent,signed-integer-overflow,unreachable,vla-bound,vptr,"
      "unsigned-integer-overflow,implicit-integer-truncation");
  args.push_back(
      "-fsanitize-recover=alignment,array-bounds,bool,builtin,enum,float-cast-"
      "overflow,float-divide-by-zero,function,integer-divide-by-zero,nonnull-"
      "attribute,null,pointer-overflow,returns-nonnull-attribute,shift-base,"
      "shift-exponent,signed-integer-overflow,vla-bound,vptr,unsigned-integer-"
      "overflow,implicit-integer-truncation");
  args.push_back(
      "-fsanitize-blacklist=/usr/lib/clang/7.0.0/share/asan_blacklist.txt");
  args.push_back("-fsanitize-address-use-after-scope");
  args.push_back("-mdisable-fp-elim");
  */
#endif
  args.push_back("-fno-assume-sane-operator-new");
  // args.push_back("-fcoroutines-ts");
  args.push_back("-stack-protector");
  args.push_back("0");
  if(opts.NoExceptions)
  {
    args.push_back("-fno-rtti");
  }
  else
  {
#if LLVM_VERSION_MAJOR <= 13
    args.push_back("-munwind-tables");
#endif

    args.push_back("-fcxx-exceptions");
    args.push_back("-fexceptions");
    args.push_back("-fexternc-nounwind");
#if defined(_WIN32)
    args.push_back("-exception-model=seh");
#endif
  }
  args.push_back("-faddrsig");

  // args.push_back("-momit-leaf-frame-pointer");
  args.push_back("-vectorize-loops");
  args.push_back("-vectorize-slp");
}

static inline void populateDefinitions(std::vector<std::string>& args)
{
#if defined(__APPLE__)
  // needed because otherwise readerwriterqueue includes CoreFoundation.h ...
  args.push_back("-DMOODYCAMEL_MAYBE_ALIGN_TO_CACHELINE=");
#endif
#define XSTR(s) STR(s)
#define STR(s) #s

#if defined(BOOST_ASIO_ENABLE_BUFFER_DEBUGGING)
  args.push_back("-DBOOST_ASIO_ENABLE_BUFFER_DEBUGGING");
#endif
#if defined(BOOST_ASIO_HAS_STD_INVOKE_RESULT)
  args.push_back(
      "-DBOOST_ASIO_HAS_STD_INVOKE_RESULT=" XSTR(BOOST_ASIO_HAS_STD_INVOKE_RESULT));
#endif
#if defined(BOOST_MATH_DISABLE_FLOAT128)
  args.push_back("-DBOOST_MATH_DISABLE_FLOAT128=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING)
  args.push_back("-DBOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING");
#endif
#if defined(BOOST_MULTI_INDEX_ENABLE_SAFE_MODE)
  args.push_back("-DBOOST_MULTI_INDEX_ENABLE_SAFE_MODE");
#endif
#if defined(BOOST_NO_RTTI)
  args.push_back("-DBOOST_NO_RTTI=" XSTR(BOOST_NO_RTTI));
#endif
#if defined(FMT_SHARED)
  args.push_back("-DFMT_SHARED=" XSTR(FMT_SHARED));
#endif
#if defined(FMT_STATIC_THOUSANDS_SEPARATOR)
  args.push_back(
      "-DFMT_STATIC_THOUSANDS_SEPARATOR=" XSTR(FMT_STATIC_THOUSANDS_SEPARATOR));
#endif
#if defined(FMT_USE_FLOAT128)
  args.push_back("-DFMT_USE_FLOAT128=" XSTR(FMT_USE_FLOAT128));
#endif
#if defined(FMT_USE_INT128)
  args.push_back("-DFMT_USE_INT128=" XSTR(FMT_USE_INT128));
#endif
#if defined(FMT_USE_LONG_DOUBLE)
  args.push_back("-DFMT_USE_LONG_DOUBLE=" XSTR(FMT_USE_LONG_DOUBLE));
#endif
#if defined(LIBREMIDI_ALSA)
  args.push_back("-DLIBREMIDI_ALSA");
#endif
#if defined(LIBREMIDI_HAS_JACK_GET_VERSION)
  args.push_back("-DLIBREMIDI_HAS_JACK_GET_VERSION");
#endif
#if defined(LIBREMIDI_HAS_UDEV)
  args.push_back("-DLIBREMIDI_HAS_UDEV");
#endif
#if defined(LIBREMIDI_JACK)
  args.push_back("-DLIBREMIDI_JACK");
#endif
#if defined(LIBREMIDI_KEYBOARD)
  args.push_back("-DLIBREMIDI_KEYBOARD");
#endif
#if defined(LIBREMIDI_PIPEWIRE)
  args.push_back("-DLIBREMIDI_PIPEWIRE");
#endif
#if defined(LIBREMIDI_PIPEWIRE_UMP)
  args.push_back("-DLIBREMIDI_PIPEWIRE_UMP");
#endif
#if defined(LIBREMIDI_USE_BOOST)
  args.push_back("-DLIBREMIDI_USE_BOOST");
#endif
#if defined(LIBREMIDI_WEAKJACK)
  args.push_back("-DLIBREMIDI_WEAKJACK");
#endif
#if defined(QT_CORE_LIB)
  args.push_back("-DQT_CORE_LIB");
#endif
#if defined(QT_DISABLE_DEPRECATED_BEFORE)
  args.push_back("-DQT_DISABLE_DEPRECATED_BEFORE=" XSTR(QT_DISABLE_DEPRECATED_BEFORE));
#endif
#if defined(QT_GUI_LIB)
  args.push_back("-DQT_GUI_LIB");
#endif
#if defined(QT_NETWORK_LIB)
  args.push_back("-DQT_NETWORK_LIB");
#endif
#if defined(QT_NO_JAVA_STYLE_ITERATORS)
  args.push_back("-DQT_NO_JAVA_STYLE_ITERATORS");
#endif
#if defined(QT_NO_KEYWORDS)
  args.push_back("-DQT_NO_KEYWORDS");
#endif
#if defined(QT_NO_LINKED_LIST)
  args.push_back("-DQT_NO_LINKED_LIST");
#endif
#if defined(QT_NO_NARROWING_CONVERSIONS_IN_CONNECT)
  args.push_back("-DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT");
#endif
#if defined(QT_NO_USING_NAMESPACE)
  args.push_back("-DQT_NO_USING_NAMESPACE");
#endif
#if defined(QT_OPENGL_LIB)
  args.push_back("-DQT_OPENGL_LIB");
#endif
#if defined(QT_QMLINTEGRATION_LIB)
  args.push_back("-DQT_QMLINTEGRATION_LIB");
#endif
#if defined(QT_QML_LIB)
  args.push_back("-DQT_QML_LIB");
#endif
#if defined(QT_SERIALPORT_LIB)
  args.push_back("-DQT_SERIALPORT_LIB");
#endif
#if defined(QT_SHADERTOOLS_LIB)
  args.push_back("-DQT_SHADERTOOLS_LIB");
#endif
#if defined(QT_STATEMACHINE_LIB)
  args.push_back("-DQT_STATEMACHINE_LIB");
#endif
#if defined(QT_USE_QSTRINGBUILDER)
  args.push_back("-DQT_USE_QSTRINGBUILDER");
#endif
#if defined(QT_WEBSOCKETS_LIB)
  args.push_back("-DQT_WEBSOCKETS_LIB");
#endif
#if defined(QT_WIDGETS_LIB)
  args.push_back("-DQT_WIDGETS_LIB");
#endif
#if defined(RAPIDJSON_HAS_STDSTRING)
  args.push_back("-DRAPIDJSON_HAS_STDSTRING=" XSTR(RAPIDJSON_HAS_STDSTRING));
#endif
#if defined(SCORE_DEBUG)
  args.push_back("-DSCORE_DEBUG");
#endif
#if defined(SCORE_LIB_BASE)
  args.push_back("-DSCORE_LIB_BASE");
#endif
#if defined(SCORE_LIB_DEVICE)
  args.push_back("-DSCORE_LIB_DEVICE");
#endif
#if defined(SCORE_LIB_INSPECTOR)
  args.push_back("-DSCORE_LIB_INSPECTOR");
#endif
#if defined(SCORE_LIB_LOCALTREE)
  args.push_back("-DSCORE_LIB_LOCALTREE");
#endif
#if defined(SCORE_LIB_PROCESS)
  args.push_back("-DSCORE_LIB_PROCESS");
#endif
#if defined(SCORE_LIB_STATE)
  args.push_back("-DSCORE_LIB_STATE");
#endif
#if defined(SCORE_PLUGIN_AUDIO)
  args.push_back("-DSCORE_PLUGIN_AUDIO");
#endif
#if defined(SCORE_PLUGIN_AUTOMATION)
  args.push_back("-DSCORE_PLUGIN_AUTOMATION");
#endif
#if defined(SCORE_PLUGIN_AVND)
  args.push_back("-DSCORE_PLUGIN_AVND");
#endif
#if defined(SCORE_PLUGIN_CURVE)
  args.push_back("-DSCORE_PLUGIN_CURVE");
#endif
#if defined(SCORE_PLUGIN_DATAFLOW)
  args.push_back("-DSCORE_PLUGIN_DATAFLOW");
#endif
#if defined(SCORE_PLUGIN_DEVICEEXPLORER)
  args.push_back("-DSCORE_PLUGIN_DEVICEEXPLORER");
#endif
#if defined(SCORE_PLUGIN_ENGINE)
  args.push_back("-DSCORE_PLUGIN_ENGINE");
#endif
#if defined(SCORE_PLUGIN_GFX)
  args.push_back("-DSCORE_PLUGIN_GFX");
#endif
#if defined(SCORE_PLUGIN_LIBRARY)
  args.push_back("-DSCORE_PLUGIN_LIBRARY");
#endif
#if defined(SCORE_PLUGIN_MEDIA)
  args.push_back("-DSCORE_PLUGIN_MEDIA");
#endif
#if defined(SCORE_PLUGIN_SCENARIO)
  args.push_back("-DSCORE_PLUGIN_SCENARIO");
#endif
#if defined(SCORE_PLUGIN_TRANSPORT)
  args.push_back("-DSCORE_PLUGIN_TRANSPORT");
#endif
#if defined(SERVUS_USE_AVAHI_CLIENT)
  args.push_back("-DSERVUS_USE_AVAHI_CLIENT");
#endif
#if defined(SPDLOG_COMPILED_LIB)
  args.push_back("-DSPDLOG_COMPILED_LIB=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_DEBUG_ON)
  args.push_back("-DSPDLOG_DEBUG_ON=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_FMT_EXTERNAL)
  args.push_back("-DSPDLOG_FMT_EXTERNAL=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_NO_DATETIME)
  args.push_back("-DSPDLOG_NO_DATETIME=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_NO_NAME)
  args.push_back("-DSPDLOG_NO_NAME=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_NO_THREAD_ID)
  args.push_back("-DSPDLOG_NO_THREAD_ID=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_SHARED_LIB)
  args.push_back("-DSPDLOG_SHARED_LIB=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(SPDLOG_TRACE_ON)
  args.push_back("-DSPDLOG_TRACE_ON=" XSTR(BOOST_MATH_DISABLE_FLOAT128));
#endif
#if defined(TINYSPLINE_DOUBLE_PRECISION)
  args.push_back("-DTINYSPLINE_DOUBLE_PRECISION");
#endif

#if defined(SCORE_DEBUG)
  args.push_back("-DSCORE_DEBUG");
#endif

  // args.push_back("-DSCORE_STATIC_PLUGINS");
  args.push_back("-D_GNU_SOURCE=1");
  args.push_back("-D__STDC_CONSTANT_MACROS");
  args.push_back("-D__STDC_FORMAT_MACROS");
  args.push_back("-D__STDC_LIMIT_MACROS");
#if defined(FFTW_SINGLE_ONLY)
  args.push_back("-DFFTW_SINGLE_ONLY");
#elif defined(FFTW_DOUBLE_ONLY)
  args.push_back("-DFFTW_DOUBLE_ONLY");
#endif
}

static inline auto getPotentialTriples()
{
  std::vector<QString> triples;
  triples.push_back(LLVM_DEFAULT_TARGET_TRIPLE);
  triples.push_back(LLVM_HOST_TRIPLE);
#if defined(__x86_64__)
  triples.push_back("x86_64-pc-linux-gnu");
#elif defined(__i686__)
  triples.push_back("i686-pc-linux-gnu");
#elif defined(__i586__)
  triples.push_back("i586-pc-linux-gnu");
#elif defined(__i486__)
  triples.push_back("i486-pc-linux-gnu");
#elif defined(__i386__)
  triples.push_back("i386-pc-linux-gnu");
#elif defined(__arm__)
  triples.push_back("armv8-none-linux-gnueabi");
  triples.push_back("armv8-pc-linux-gnueabi");
  triples.push_back("armv8-none-linux-gnu");
  triples.push_back("armv8-pc-linux-gnu");
  triples.push_back("armv7-none-linux-gnueabi");
  triples.push_back("armv7-pc-linux-gnueabi");
  triples.push_back("armv7-none-linux-gnu");
  triples.push_back("armv7-pc-linux-gnu");
  triples.push_back("armv6-none-linux-gnueabi");
  triples.push_back("armv6-pc-linux-gnueabi");
  triples.push_back("armv6-none-linux-gnu");
  triples.push_back("armv6-pc-linux-gnu");
#elif defined(__aarch64__)
  triples.push_back("aarch64-none-linux-gnueabi");
  triples.push_back("aarch64-pc-linux-gnueabi");
  triples.push_back("aarch64-none-linux-gnu");
  triples.push_back("aarch64-pc-linux-gnu");
  triples.push_back("aarch64-redhat-linux");
#endif

  return triples;
}
/**
 * @brief populateIncludeDirs Add paths to the relevant parts of the sdk
 *
 * On windows :
 *
 * folder/score.exe
 * -resource-dir folder/sdk/clang/7.0.0             -> clang stuff
 * -internal-isystem folder/sdk/include/c++/v1      -> libc++
 * -internal-isystem folder/sdk/clang/7.0.0/include -> clang stuff
 * -internal-externc-isystem folder/sdk/include     -> Qt, mingw, ffmpeg, etc.
 * -I folder/sdk/include/score                      -> score headers (ossia
 * folder should be in include)
 *
 * On mac :
 * score.app/Contents/macOS/score
 * -resource-dir score.app/Contents/Frameworks/Score.Framework/clang/7.0.0
 * -internal-isystem
 * score.app/Contents/Frameworks/Score.Framework/include/c++/v1
 * -internal-isystem
 * score.app/Contents/Frameworks/Score.Framework/clang/7.0.0/include
 * -internal-externc-isystem
 * score.app/Contents/Frameworks/Score.Framework/include -I
 * score.app/Contents/Frameworks/Score.Framework/include/score
 *
 * On linux :
 * /tmp/.mount/usr/bin/score
 * -resource-dir /tmp/.mount/usr/lib/clang/7.0.0
 * -internal-isystem /tmp/.mount/usr/include/c++/v1
 * -internal-isystem /tmp/.mount/usr/lib/clang/7.0.0/include
 * -internal-externc-isystem /tmp/.mount/usr/include
 * -I /tmp/.mount/usr/include/score
 *
 * Generally :
 * -resource-dir $SDK_ROOT/lib/clang/7.0.0
 * -internal-isystem $SDK_ROOT/include/c++/v1
 * -internal-isystem $SDK_ROOT/lib/clang/7.0.0/include
 * -internal-externc-isystem $SDK_ROOT/include
 * -I $SDK_ROOT/include/score
 */
static inline void populateIncludeDirs(std::vector<std::string>& args)
{
  auto sdk = locateSDK();
  auto qsdk = QString::fromStdString(sdk);
  std::cerr << "\nLooking for sdk in: " << sdk << "\n";
  qDebug() << "Looking for sdk in: " << qsdk;

  bool sdk_found = true;

  {
    QDir dir(qsdk);
    if(!dir.cd("include") || !dir.cd("c++"))
    {
      qDebug() << "Unable to locate standard headers, fallback to /usr";
      qsdk = "/usr";
      sdk = "/usr";
      dir.setPath("/usr");
      if(!dir.cd("include") || !dir.cd("c++"))
      {
        qDebug() << "Unable to locate standard headers++";
        throw std::runtime_error("Unable to compile");
      }
      sdk_found = false;
    }
  }

  qDebug() << "SDK located: " << qsdk;
  std::string llvm_lib_version = SCORE_LLVM_VERSION;
#if defined(__APPLE__) && SCORE_FHS_BUILD
  llvm_lib_version = "13.0.0";
#endif

  QDir resDir = QString(qsdk + "/lib/clang");
  auto entries = resDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  if(!entries.empty() && !entries.contains(SCORE_LLVM_VERSION))
    llvm_lib_version = entries.front().toStdString();

#if defined(__APPLE__) && SCORE_FHS_BUILD
  std::string appleSharedSdk
      = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/"
        "usr";
  args.push_back("-resource-dir");
  args.push_back(appleSharedSdk + "/lib/clang/" + llvm_lib_version);
#else
  args.push_back("-resource-dir");
  args.push_back(sdk + "/lib/clang/" + llvm_lib_version);
#endif

#if defined(_LIBCPP_VERSION)
  args.push_back("-stdlib=libc++");
  args.push_back("-internal-isystem");

#if defined(__APPLE__) && SCORE_FHS_BUILD
  args.push_back(appleSharedSdk + "/include/c++/v1");
#else
  args.push_back(sdk + "/include/c++/v1");
#endif

#elif defined(_GLIBCXX_RELEASE)
  // Try to locate the correct libstdc++ folder
  // TODO these are only heuristics. how to make them better ?
  {
    const auto libstdcpp_major = QString::number(_GLIBCXX_RELEASE);

    QDir cpp_dir{"/usr/include/c++"};
    // Note: as this is only used for debugging we look in the host /usr
    QDirIterator cpp_it{cpp_dir};
    while(cpp_it.hasNext())
    {
      cpp_it.next();
      auto ver = cpp_it.fileName();
      if(!ver.isEmpty() && ver.startsWith(libstdcpp_major))
      {
        auto gcc = ver.toStdString();

        // e.g. /usr/include/c++/8.2.1
        args.push_back("-internal-isystem");
        args.push_back("/usr/include/c++/" + gcc);

        cpp_dir.cd(ver);
        for(auto& triple : getPotentialTriples())
        {
          if(cpp_dir.exists(triple))
          {
            // e.g. /usr/include/c++/8.2.1/x86_64-pc-linux-gnu
            args.push_back("-internal-isystem");
            args.push_back("/usr/include/c++/" + gcc + "/" + triple.toStdString());
            break;
          }
        }

        break;
      }
    }
  }
#endif

#if defined(__APPLE__) && SCORE_FHS_BUILD
  args.push_back("-internal-isystem");
  args.push_back(appleSharedSdk + "/lib/clang/" + llvm_lib_version + "/include");
  args.push_back("-internal-externc-isystem");
  args.push_back(
      "/Applications/Xcode.app/Contents/Developer//Platforms/MacOSX.platform/Developer/"
      "SDKs/MacOSX.sdk/usr/include");
#else
  args.push_back("-internal-isystem");
  args.push_back(sdk + "/lib/clang/" + llvm_lib_version + "/include");
  args.push_back("-internal-externc-isystem");
  args.push_back(sdk + "/include");
#endif

  // -resource-dir
  // /opt/score-sdk/llvm/lib/clang/11.0.0
  // -internal-isystem
  // /opt/score-sdk/llvm/bin/../include/c++/v1
  // -internal-isystem
  // /opt/score-sdk/llvm/lib/clang/11.0.0/include
  //-internal-externc-isystem
  ///build/score.AppDir/usr/include/

  auto include = [&](const std::string& path) {
    args.push_back("-isystem" + sdk + "/include/" + path);
  };

#if defined(__linux__)
  include("x86_64-linux-gnu"); // #debian
#endif
  // include(""); // /usr/include
  std::string qt_folder = "qt";
  if(QFile::exists(qsdk + "/include/qt6/QtCore"))
    qt_folder = "qt6";

  QDirIterator qtVersionFolder{
      qsdk + "/include/" + QString::fromStdString(qt_folder) + "/QtCore",
      {},
      QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot,
      {}};
  std::string qt_version = QT_VERSION_STR;
  if(qtVersionFolder.hasNext())
  {
    QDir sub = qtVersionFolder.next();
    qt_version = sub.dirName().toStdString();
  }
  include(qt_folder + "");
  include(qt_folder + "/QtCore");
  include(qt_folder + "/QtCore/" + qt_version);
  include(qt_folder + "/QtCore/" + qt_version + "/QtCore");
  include(qt_folder + "/QtGui");
  include(qt_folder + "/QtGui/" + qt_version);
  include(qt_folder + "/QtGui/" + qt_version + "/QtGui");
  include(qt_folder + "/QtWidgets");
  include(qt_folder + "/QtWidgets/" + qt_version);
  include(qt_folder + "/QtWidgets/" + qt_version + "/QtWidgets");
  include(qt_folder + "/QtQml");
  include(qt_folder + "/QtQml/" + qt_version);
  include(qt_folder + "/QtQml/" + qt_version + "/QtQml");
  include(qt_folder + "/QtQuick");
  include(qt_folder + "/QtQuick/" + qt_version);
  include(qt_folder + "/QtQuick/" + qt_version + "/QtQuick");
  include(qt_folder + "/QtXml");
  include(qt_folder + "/QtNetwork");
  include(qt_folder + "/QtSvg");
  include(qt_folder + "/QtSql");
  include(qt_folder + "/QtOpenGL");
  include(qt_folder + "/QtShaderTools");
  include(qt_folder + "/QtShaderTools/" + qt_version);
  include(qt_folder + "/QtShaderTools/" + qt_version + "/QtShaderTools");
  include(qt_folder + "/QtSerialBus");
  include(qt_folder + "/QtSerialPort");

#if !SCORE_FHS_BUILD
  bool deploying = true;
#else
  bool deploying = false;
#endif

  if(deploying && sdk_found)
  {
    include("score");
  }
  else
  {
    auto thirdparty_include_dirs = {
        "/3rdparty/libossia/3rdparty/boost_1_88_0",
        "/3rdparty/libossia/3rdparty/nano-signal-slot/include",
        "/3rdparty/libossia/3rdparty/spdlog/include",
        "/3rdparty/libossia/3rdparty/dr_libs",
        "/3rdparty/libossia/3rdparty/Flicks",
        "/3rdparty/libossia/3rdparty/fmt/include",
        "/3rdparty/libossia/3rdparty/magic_enum/include",
        "/3rdparty/libossia/3rdparty/readerwriterqueue",
        "/3rdparty/libossia/3rdparty/concurrentqueue",
        "/3rdparty/libossia/3rdparty/SmallFunction/smallfun/include",
        "/3rdparty/libossia/3rdparty/websocketpp",
        "/3rdparty/libossia/3rdparty/rapidjson/include",
        "/3rdparty/libossia/3rdparty/libremidi/include",
        "/3rdparty/libossia/3rdparty/oscpack",
        "/3rdparty/libossia/3rdparty/rnd/include",
        "/3rdparty/libossia/3rdparty/span/include",
        "/3rdparty/libossia/3rdparty/tuplet/include",
        "/3rdparty/libossia/3rdparty/unordered_dense/include",
        "/3rdparty/libossia/3rdparty/multi_index/include",
        "/3rdparty/libossia/3rdparty/verdigris/src",
        "/3rdparty/libossia/3rdparty/weakjack",
    };
    for(auto path : thirdparty_include_dirs)
    {
      args.push_back("-isystem" + std::string(SCORE_ROOT_SOURCE_DIR) + path);
    }
    auto src_include_dirs
        = {"/3rdparty/libossia/src",
           "/3rdparty/avendish/include",
           "/src/lib",
           "/src/plugins/score-lib-state",
           "/src/plugins/score-lib-device",
           "/src/plugins/score-lib-process",
           "/src/plugins/score-lib-inspector",
           "/src/plugins/score-plugin-avnd",
           "/src/plugins/score-plugin-gfx",
           "/src/plugins/score-plugin-jit",
           "/src/plugins/score-plugin-nodal",
           "/src/plugins/score-plugin-remotecontrol",
           "/src/plugins/score-plugin-audio",
           "/src/plugins/score-plugin-curve",
           "/src/plugins/score-plugin-dataflow",
           "/src/plugins/score-plugin-engine",
           "/src/plugins/score-plugin-scenario",
           "/src/plugins/score-plugin-library",
           "/src/plugins/score-plugin-deviceexplorer",
           "/src/plugins/score-plugin-media",
           "/src/plugins/score-plugin-loop",
           "/src/plugins/score-plugin-midi",
           "/src/plugins/score-plugin-protocols",
           "/src/plugins/score-plugin-recording",
           "/src/plugins/score-plugin-automation",
           "/src/plugins/score-plugin-js",
           "/src/plugins/score-plugin-mapping"};

    for(auto path : src_include_dirs)
    {
      args.push_back("-I" + std::string(SCORE_ROOT_SOURCE_DIR) + path);
    }

    auto src_build_dirs
        = {"/.",
           "/src/lib",
           "/src/plugins/score-lib-state",
           "/src/plugins/score-lib-device",
           "/src/plugins/score-lib-process",
           "/src/plugins/score-lib-inspector",
           "/src/plugins/score-plugin-avnd",
           "/src/plugins/score-plugin-gfx",
           "/src/plugins/score-plugin-jit",
           "/src/plugins/score-plugin-nodal",
           "/src/plugins/score-plugin-remotecontrol",
           "/src/plugins/score-plugin-audio",
           "/src/plugins/score-plugin-curve",
           "/src/plugins/score-plugin-dataflow",
           "/src/plugins/score-plugin-engine",
           "/src/plugins/score-plugin-scenario",
           "/src/plugins/score-plugin-library",
           "/src/plugins/score-plugin-deviceexplorer",
           "/src/plugins/score-plugin-media",
           "/src/plugins/score-plugin-loop",
           "/src/plugins/score-plugin-midi",
           "/src/plugins/score-plugin-protocols",
           "/src/plugins/score-plugin-recording",
           "/src/plugins/score-plugin-automation",
           "/src/plugins/score-plugin-js",
           "/src/plugins/score-plugin-mapping",
           "/3rdparty/libossia/src"};

    for(auto path : src_build_dirs)
    {
      args.push_back("-I" + std::string(SCORE_ROOT_BINARY_DIR) + path);
    }
  }
}

}
