#pragma once
#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <Library/LibrarySettings.hpp>

#include <llvm/Support/Host.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>
#include <ciso646>
#include <iostream>
#include <string>
#include <vector>
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
#include <JitCpp/JitOptions.hpp>
#include <score_git_info.hpp>
namespace Jit
{

static inline std::string locateSDK()
{
  auto& ctx = score::AppContext().settings<Library::Settings::Model>();
  QString path = ctx.getPath() + "/SDK/";

  if(QString libPath = path + QString(SCORE_TAG_NO_V) + "/usr"; QDir(libPath + "/include/c++").exists())
  {
    return libPath.toStdString();
  }

  if(QString libPath = path + "/usr"; QDir(libPath + "/include/c++").exists())
  {
    return libPath.toStdString();
  }

  auto appFolder = qApp->applicationDirPath();

#if defined(SCORE_DEPLOYMENT_BUILD)

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
    return QString(appFolder + "/Score.Framework").toStdString();
  }
#endif

#else
  if (QFileInfo("/usr/include/c++").isDir())
  {
    return "/usr";
  }
  else
  {
    auto libpath
        = score::AppContext().settings<Library::Settings::Model>().getPath();
    libpath += "/sdk";
    return libpath.toStdString();
  }
#endif
}

static inline void populateCompileOptions(std::vector<std::string>& args, CompilerOptions opts)
{
  args.push_back("-triple");
  args.push_back(llvm::sys::getDefaultTargetTriple());
  args.push_back("-target-cpu");
  args.push_back(llvm::sys::getHostCPUName().lower());

  {
    llvm::StringMap<bool> HostFeatures;
    if (llvm::sys::getHostCPUFeatures(HostFeatures))
    {
      for (const llvm::StringMapEntry<bool> &F : HostFeatures)
      {
        args.push_back("-target-feature");
        args.push_back((F.second ? "+" : "-") + F.first().str());
      }
    }
  }


  args.push_back("-std=c++2a");
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
  args.push_back("-menable-unsafe-fp-math");
  args.push_back("-fno-signed-zeros");
  args.push_back("-mreassociate");
  args.push_back("-freciprocal-math");
  args.push_back("-fno-rounding-math");
  args.push_back("-fno-trapping-math");
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

  args.push_back("-fgnuc-version=10.0.1");

  args.push_back("-mrelocation-model");
  args.push_back("pic");
  args.push_back("-pic-level");
  args.push_back("2");
  //args.push_back("-pic-is-pie");

  args.push_back("-fvisibility");
  args.push_back("hidden");

  args.push_back("-fvisibility-inlines-hidden");

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
  args.push_back("-stack-protector");
  args.push_back("0");
  if(opts.NoExceptions)
  {
    args.push_back("-fno-rtti");
  }
  else
  {
    args.push_back("-munwind-tables");
    args.push_back("-fcxx-exceptions");
    args.push_back("-fexceptions");
#if defined(_WIN32)
    args.push_back("-fseh-exceptions");
#endif
  }
  args.push_back("-faddrsig");

  // args.push_back("-momit-leaf-frame-pointer");
  args.push_back("-vectorize-loops");
  args.push_back("-vectorize-slp");
}

static inline void populateDefinitions(std::vector<std::string>& args)
{
  args.push_back("-DASIO_STANDALONE=1");
  args.push_back("-DBOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING");
  args.push_back("-DBOOST_MULTI_INDEX_ENABLE_SAFE_MODE");
  args.push_back("-DQT_CORE_LIB");
  args.push_back("-DQT_DISABLE_DEPRECATED_BEFORE=0x050800");
  args.push_back("-DQT_GUI_LIB");
  args.push_back("-DQT_NETWORK_LIB");
  args.push_back("-DQT_NO_KEYWORDS");
  args.push_back("-DQT_QML_LIB");
  args.push_back("-DQT_QUICK_LIB");
  args.push_back("-DQT_SERIALPORT_LIB");
  args.push_back("-DQT_STATICPLUGIN");
  args.push_back("-DQT_SVG_LIB");
  args.push_back("-DQT_WEBSOCKETS_LIB");
  args.push_back("-DQT_WIDGETS_LIB");
  args.push_back("-DRAPIDJSON_HAS_STDSTRING=1");
  args.push_back("-DSCORE_DEBUG");
  args.push_back("-DSCORE_LIB_BASE");
  args.push_back("-DSCORE_LIB_DEVICE");
  args.push_back("-DSCORE_LIB_INSPECTOR");
  args.push_back("-DSCORE_LIB_LOCALTREE");
  args.push_back("-DSCORE_LIB_PROCESS");
  args.push_back("-DSCORE_LIB_STATE");
  args.push_back("-DSCORE_ADDON_GFX");
  args.push_back("-DSCORE_ADDON_REMOTECONTROL");
  args.push_back("-DSCORE_ADDON_NODAL");
  args.push_back("-DSCORE_PLUGIN_AUDIO");
  args.push_back("-DSCORE_PLUGIN_AUTOMATION");
  args.push_back("-DSCORE_PLUGIN_CURVE");
  args.push_back("-DSCORE_PLUGIN_DATAFLOW");
  args.push_back("-DSCORE_PLUGIN_DEVICEEXPLORER");
  args.push_back("-DSCORE_PLUGIN_ENGINE");
  args.push_back("-DSCORE_PLUGIN_LIBRARY");
  args.push_back("-DSCORE_PLUGIN_MAPPING");
  args.push_back("-DSCORE_PLUGIN_MEDIA");
  args.push_back("-DSCORE_PLUGIN_PROTOCOLS");
  args.push_back("-DSCORE_PLUGIN_SCENARIO");
  args.push_back("-DSCORE_PLUGIN_MIDI");
  args.push_back("-DSCORE_PLUGIN_RECORDING");
  // args.push_back("-DSCORE_STATIC_PLUGINS");
  args.push_back("-DTINYSPLINE_DOUBLE_PRECISION");
  args.push_back("-D_GNU_SOURCE");
  args.push_back("-D__STDC_CONSTANT_MACROS");
  args.push_back("-D__STDC_FORMAT_MACROS");
  args.push_back("-D__STDC_LIMIT_MACROS");
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
    if (!dir.cd("include") || !dir.cd("c++"))
    {
      qDebug() << "Unable to locate standard headers, fallback to /usr";
      sdk = "/usr";
      dir.setPath("/usr");
      if (!dir.cd("include") || !dir.cd("c++"))
      {
        qDebug() << "Unable to locate standard headers++";
        throw std::runtime_error("Unable to compile");
      }
      sdk_found = false;
    }
  }

  qDebug() << "SDK located: " << qsdk;
  std::string llvm_lib_version = SCORE_LLVM_VERSION;

  QDir resDir = QString(qsdk + "/lib/clang");
  auto entries = resDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  if(!entries.empty())
    llvm_lib_version = entries.front().toStdString();

  args.push_back("-resource-dir");
  args.push_back(sdk + "/lib/clang/" + llvm_lib_version);

#if defined(_LIBCPP_VERSION)
  args.push_back("-internal-isystem");
  args.push_back(sdk + "/include/c++/v1");
#elif defined(_GLIBCXX_RELEASE)
  // Try to locate the correct libstdc++ folder
  // TODO these are only heuristics. how to make them better ?
  {
    const auto libstdcpp_major = QString::number(_GLIBCXX_RELEASE);

    QDir cpp_dir{"/usr/include/c++"};
    // Note: as this is only used for debugging we look in the host /usr
    QDirIterator cpp_it{cpp_dir};
    while (cpp_it.hasNext())
    {
      cpp_it.next();
      auto ver = cpp_it.fileName();
      if (!ver.isEmpty() && ver.startsWith(libstdcpp_major))
      {
        auto gcc = ver.toStdString();

        // e.g. /usr/include/c++/8.2.1
        args.push_back("-internal-isystem");
        args.push_back("/usr/include/c++/" + gcc);

        cpp_dir.cd(ver);
        for (auto& triple : getPotentialTriples())
        {
          if (cpp_dir.exists(triple))
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

  args.push_back("-internal-isystem");
  args.push_back(sdk + "/lib/clang/" + llvm_lib_version + "/include");

  args.push_back("-internal-externc-isystem");
  args.push_back(sdk + "/include");


  // -resource-dir
  // /opt/score-sdk/llvm/lib/clang/11.0.0
  // -internal-isystem
  // /opt/score-sdk/llvm/bin/../include/c++/v1
  // -internal-isystem
  // /opt/score-sdk/llvm/lib/clang/11.0.0/include
  //-internal-externc-isystem
  ///build/score.AppDir/usr/include/


  auto include = [&](const std::string& path) {
    args.push_back("-I" + sdk + "/include/" + path);
  };


#if defined(__linux__)
  include("x86_64-linux-gnu"); // #debian
#endif
  // include(""); // /usr/include
  QDirIterator qtVersionFolder{qsdk + "/include/qt/QtCore", {}, QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot, {}};
  std::string qt_version = QT_VERSION_STR;
  if(qtVersionFolder.hasNext()) {
    QDir sub = qtVersionFolder.next();
    qt_version = sub.dirName().toStdString();
  }
  include("qt");
  include("qt/QtCore");
  include("qt/QtCore/" + qt_version);
  include("qt/QtCore/" + qt_version + "/QtCore");
  include("qt/QtGui");
  include("qt/QtGui/" + qt_version);
  include("qt/QtGui/" + qt_version + "/QtGui");
  include("qt/QtWidgets");
  include("qt/QtWidgets/" + qt_version);
  include("qt/QtWidgets/" + qt_version + "/QtWidgets");
  include("qt/QtXml");
  include("qt/QtQml");
  include("qt/QtNetwork");
  include("qt/QtSvg");
  include("qt/QtSql");
  include("qt/QtOpenGL");
  include("qt/QtSerialBus");
  include("qt/QtSerialPort");

#if defined(SCORE_DEPLOYMENT_BUILD)
  bool deploying = true;
#else
  bool deploying = false;
#endif

  if (deploying && sdk_found)
  {
    include("score");
  }
  else
  {
    auto src_include_dirs = {"/3rdparty/libossia/src",
                             "/3rdparty/libossia/3rdparty/boost_1_73_0",
                             "/3rdparty/libossia/3rdparty/variant/include",
                             "/3rdparty/libossia/3rdparty/nano-signal-slot/include",
                             "/3rdparty/libossia/3rdparty/spdlog/include",
                             "/3rdparty/libossia/3rdparty/brigand/include",
                             "/3rdparty/libossia/3rdparty/Flicks",
                             "/3rdparty/libossia/3rdparty/fmt/include",
                             "/3rdparty/libossia/3rdparty/hopscotch-map/include",
                             "/3rdparty/libossia/3rdparty/chobo-shl/include",
                             "/3rdparty/libossia/3rdparty/frozen/include",
                             "/3rdparty/libossia/3rdparty/bitset2",
                             "/3rdparty/libossia/3rdparty/GSL/include",
                             "/3rdparty/libossia/3rdparty/flat_hash_map",
                             "/3rdparty/libossia/3rdparty/flat/include",
                             "/3rdparty/libossia/3rdparty/readerwriterqueue",
                             "/3rdparty/libossia/3rdparty/concurrentqueue",
                             "/3rdparty/libossia/3rdparty/SmallFunction/smallfun/include",
                             "/3rdparty/libossia/3rdparty/asio/asio/include",
                             "/3rdparty/libossia/3rdparty/websocketpp",
                             "/3rdparty/libossia/3rdparty/rapidjson/include",
                             "/3rdparty/libossia/3rdparty/libremidi/include",
                             "/3rdparty/libossia/3rdparty/oscpack",
                             "/3rdparty/libossia/3rdparty/multi_index/include",
                             "/3rdparty/libossia/3rdparty/verdigris/src",
                             "/3rdparty/libossia/3rdparty/weakjack",
                             "/src/lib",
                             "/src/plugins/score-lib-state",
                             "/src/plugins/score-lib-device",
                             "/src/plugins/score-lib-process",
                             "/src/plugins/score-lib-inspector",
                             "/src/plugins/score-addon-gfx",
                             "/src/plugins/score-addon-jit",
                             "/src/plugins/score-addon-nodal",
                             "/src/plugins/score-addon-remotecontrol",
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

    for (auto path : src_include_dirs)
    {
      args.push_back("-I" + std::string(SCORE_ROOT_SOURCE_DIR) + path);
    }

    auto src_build_dirs = {"/.",
                           "/src/lib",
                           "/src/plugins/score-lib-state",
                           "/src/plugins/score-lib-device",
                           "/src/plugins/score-lib-process",
                           "/src/plugins/score-lib-inspector",
                           "/src/plugins/score-addon-gfx",
                           "/src/plugins/score-addon-jit",
                           "/src/plugins/score-addon-nodal",
                           "/src/plugins/score-addon-remotecontrol",
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

    for (auto path : src_build_dirs)
    {
      args.push_back("-I" + std::string(SCORE_ROOT_BINARY_DIR) + path);
    }
  }
}

}
