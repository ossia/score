// What the JIT puts on its compiler's command line from what is on disk and
// from score's build tree, on fake directory trees.
#include <JitCpp/JitBuildTree.hpp>
#include <JitCpp/JitSearchPaths.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

namespace
{
void touch(const QString& path)
{
  QDir().mkpath(QFileInfo(path).path());
  QFile f(path);
  REQUIRE(f.open(QIODevice::WriteOnly));
}
}

TEST_CASE("The clang resource directory is the one of the JIT's major version", "[jit]")
{
  QTemporaryDir root;
  REQUIRE(root.isValid());
  const QString clang = root.path() + "/lib/clang";
  QDir().mkpath(clang + "/22");
  QDir().mkpath(clang + "/23");

  // Debian with LLVM 22 and 23: "22" sorts first
  CHECK(Jit::selectClangResourceDir(clang, 23, "23.1.3") == "23");
  CHECK(Jit::selectClangResourceDir(clang, 22, "22.1.0") == "22");
  CHECK(Jit::selectClangResourceDir(clang, 24, "24.0.0").isEmpty());
  CHECK(Jit::newestClangResourceDir(clang) == "23");

  SECTION("Before LLVM 16 it is named after the full version")
  {
    QDir().mkpath(clang + "/15.0.7");
    QDir().mkpath(clang + "/14.0.6");
    CHECK(Jit::selectClangResourceDir(clang, 15, "15.0.7") == "15.0.7");
    CHECK(Jit::selectClangResourceDir(clang, 15, "15.0.6") == "15.0.7");
    CHECK(Jit::selectClangResourceDir(clang, 14, "14.0.6") == "14.0.6");
  }
}

TEST_CASE("The compiler-rt archives are those of the JIT's clang and architecture", "[jit]")
{
  QTemporaryDir root;
  REQUIRE(root.isValid());
  const QString r = root.path();
  touch(r + "/lib/clang/22/lib/x86_64-pc-linux-gnu/liborc_rt.a");
  touch(r + "/lib/clang/23/lib/i386-unknown-linux-gnu/liborc_rt.a");
  touch(r + "/lib/clang/23/lib/x86_64-unknown-linux-gnu/liborc_rt.a");

  CHECK(
      Jit::findClangRuntimeArchive({r}, 23, "23.1.3", "liborc_rt*.a", {"/x86_64-"})
      == r + "/lib/clang/23/lib/x86_64-unknown-linux-gnu/liborc_rt.a");
  CHECK(
      Jit::findClangRuntimeArchive({r}, 22, "22.1.0", "liborc_rt*.a", {"/x86_64-"})
      == r + "/lib/clang/22/lib/x86_64-pc-linux-gnu/liborc_rt.a");

  // An orc_rt of another clang is not compatible with the JIT's
  CHECK(Jit::findClangRuntimeArchive({r}, 24, "24.0.0", "liborc_rt*.a").isEmpty());

  SECTION("A root without the JIT's clang does not hide the next one")
  {
    QTemporaryDir other;
    REQUIRE(other.isValid());
    touch(other.path() + "/lib/clang/21/lib/linux/liborc_rt-x86_64.a");
    CHECK(
        Jit::findClangRuntimeArchive(
            {other.path(), r}, 23, "23.1.3", "liborc_rt*.a", {"/x86_64-"})
        == r + "/lib/clang/23/lib/x86_64-unknown-linux-gnu/liborc_rt.a");
  }

  SECTION("The COFF builtins are preferred")
  {
    touch(r + "/lib/clang/23/lib/linux/libclang_rt.builtins-x86_64.a");
    touch(r + "/lib/clang/23/lib/windows/libclang_rt.builtins-x86_64.a");
    CHECK(
        Jit::findClangRuntimeArchive(
            {r}, 23, "23.1.3", "libclang_rt.builtins-x86_64.a", {"/windows/"})
        == r + "/lib/clang/23/lib/windows/libclang_rt.builtins-x86_64.a");
    CHECK(
        Jit::findClangRuntimeArchive({r}, 23, "23.1.3", "libclang_rt.builtins-x86_64.a")
        == r + "/lib/clang/23/lib/linux/libclang_rt.builtins-x86_64.a");
  }
}

TEST_CASE("The multiarch directory is the host's", "[jit]")
{
  QTemporaryDir root;
  REQUIRE(root.isValid());
  const QString inc = root.path() + "/include";
  const QString marker = "c++/15/bits/c++config.h";
  touch(inc + "/x86_64-linux-gnu/" + marker);

  CHECK(Jit::withoutVendor("x86_64-pc-linux-gnu") == "x86_64-linux-gnu");
  CHECK(Jit::withoutVendor("x86_64-linux-gnu") == "x86_64-linux-gnu");

  // A single candidate is taken whatever its name
  CHECK(Jit::selectMultiarchDir(inc, marker, {"x86_64-redhat-linux"}) == "x86_64-linux-gnu");

  // With multilib, i386-linux-gnu sorts first
  touch(inc + "/i386-linux-gnu/" + marker);
  CHECK(
      Jit::selectMultiarchDir(inc, marker, {"x86_64-pc-linux-gnu", "i386-pc-linux-gnu"})
      == "x86_64-linux-gnu");
  CHECK(Jit::selectMultiarchDir(inc, marker, {"i386-pc-linux-gnu"}) == "i386-linux-gnu");
  CHECK(Jit::selectMultiarchDir(inc, marker, {"aarch64-unknown-linux-gnu"}).isEmpty());

  // libc++'s __config_site, named after the full triple
  touch(inc + "/x86_64-unknown-linux-gnu/c++/v1/__config_site");
  CHECK(
      Jit::selectMultiarchDir(inc, "c++/v1/__config_site", {"x86_64-unknown-linux-gnu"})
      == "x86_64-unknown-linux-gnu");
}

TEST_CASE("The build tree's include directories", "[jit]")
{
  const std::vector<std::string_view> dirs{
      "/src/score/src/lib",
      "",
      "/build/score/src/lib",
      "/opt/Qt/lib/QtCore.framework",
      "/opt/Qt/lib/QtCore.framework/Headers",
      "/opt/Qt/lib/QtGui.framework/",
      "/usr/include/eigen3",
      "/sdk/usr/include",
      "/src/score/src/lib",
  };
  const auto args = Jit::buildTreeIncludeArgs(
      dirs, "/src/score/src/", "/build/score", "/sdk/usr/include/");
  const std::vector<std::string> expected{
      "-I/src/score/src/lib",
      "-I/build/score/src/lib",
      // <QtCore/qglobal.h> resolves in QtCore.framework/Headers
      "-iframework/opt/Qt/lib",
      "-isystem/opt/Qt/lib/QtCore.framework/Headers",
      "-isystem/usr/include/eigen3",
  };
  CHECK(args == expected);
}

TEST_CASE("A definition already in effect is not repeated", "[jit]")
{
  std::vector<std::string> args{
      "-D_GNU_SOURCE", "-target-feature", "+avx", "-D_GNU_SOURCE=1",
      "-DFOO=2",       "-DFOO=3",         "-UFOO", "-DFOO=3",
      "-DBAR=",        "-DBAR=",
  };
  Jit::removeRepeatedDefinitions(args);
  const std::vector<std::string> expected{
      "-D_GNU_SOURCE", "-target-feature", "+avx", "-DFOO=2", "-DFOO=3",
      "-UFOO",         "-DFOO=3",         "-DBAR=",
  };
  CHECK(args == expected);
}

// The generated values are reached through the plug-in, not through a header a
// dependent or an SDK would need the generated file for: this file includes
// JitBuildTree.hpp without the generated header's directory.
TEST_CASE("The build tree's values are those score was configured with", "[jit]")
{
  const auto has = [](auto range, std::string_view v) {
    return std::any_of(range.begin(), range.end(), [&](const char* e) {
      return std::string_view{e} == v;
    });
  };

  const std::string lib = std::string(SCORE_ROOT_SOURCE_DIR) + "/src/lib";
  CHECK(has(Jit::buildTreeIncludeDirs(), lib));

  // A PUBLIC definition of the JIT plug-in, whose value is a path
  CHECK(std::any_of(
      Jit::buildTreeDefinitions().begin(), Jit::buildTreeDefinitions().end(),
      [](const char* e) {
        return std::string_view{e}.starts_with("SCORE_ROOT_SOURCE_DIR=");
      }));

  // Export macros are the plug-ins' own
  CHECK(std::none_of(
      Jit::buildTreeDefinitions().begin(), Jit::buildTreeDefinitions().end(),
      [](const char* e) { return std::string_view{e}.ends_with("_EXPORTS"); }));
}
