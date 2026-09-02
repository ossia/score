// =============================================================================
// P1-16 / G16 -- the academy shader-include path is registered, or its
// absence is diagnosable.
//
// The 11 real 2026/lgm/* scores' shaders say `#include "openpbr.h"` and rely
// on the header being reachable through score's shader-include search path.
// This file pins down what that search path actually IS, that it is live
// (re-read on every resolution, so installing the package heals a previously
// failing shader without a restart), and that a shader whose include cannot
// resolve fails with a message naming the header and every directory that was
// searched -- never a silent black frame.
//
// The mechanism, as found (all in
// src/plugins/score-plugin-gfx/Gfx/ShaderProgram.cpp):
//
//   * shaderIncludePaths() (ShaderProgram.cpp:23-56) builds the search list
//     fresh on every call: the Library packages directory
//     (Library::Settings::Model::getPackagesPath() == RootPath + "/packages",
//     LibrarySettings.cpp:149-152) plus every first-level subdirectory of it,
//     so a shader library shipping as a package (openpbr/, lygia/, ...) is
//     includable by bare header name. There is NO separate runtime
//     registration API: the comment at ShaderProgram.cpp:26-31 states "no
//     static registration mechanism lives here anymore". "Registering" the
//     academy include path therefore means: a package directory containing
//     openpbr.h exists under the library's packages/ dir. That is what
//     score-addon-academy's environment provides on machines that have it.
//
//   * A quoted include that resolves nowhere is a HARD, NAMED failure:
//     resolveIncludes() (ShaderProgram.cpp:317-326) sets
//       Shader include not found: "<header>" (searched: <path list>)
//     and aborts the expansion. ProgramCache::get() forwards it prefixed
//     with the stage ("Fragment: " at ShaderProgram.cpp:482, "Vertex: " at
//     :493) and returns std::nullopt -- the caller gets a diagnostic, not a
//     frame. Because this is already diagnosable, nothing here is pinned
//     [!shouldfail] (contrast tests/unit/AssetLoaderFailure.cpp, where the
//     silent path had to be pinned as a defect).
//
//   * Included files are framed with `#line 1 "<canonical path>"` markers
//     (ShaderProgram.cpp:246) so a glslang error inside the header names the
//     header, not the including shader.
//
//   * Failures are NOT cached: ProgramCache::get() inserts into `programs`
//     only on success (ShaderProgram.cpp:561), so the same source recompiles
//     cleanly once the package appears.
//
// GPU-less: everything up to and including the compile goes through
// QShaderBaker (score::gfx::ShaderCache::get), which cross-compiles on any
// host with no device and no display -- the same no-device pattern as
// tests/gfx/GfxCsfOrientGate.cpp. The graphics API is pinned to Vulkan for
// the compile leg because Gfx::Settings::shaderVersionForAPI(OpenGL)
// constructs score::GLCapabilities{}, which wants a GL context
// (Gfx/Settings/Model.cpp:266-267); the Vulkan answer is the constant
// QShaderVersion(100) (Model.cpp:269-271) and QShaderBaker always emits
// SPIR-V regardless of whether the host has Vulkan.
//
// The positive openpbr half ("with the addon"): score-addon-academy is not
// part of this checkout -- src/addons/ holds aja/lavfi/ndi/openzen/synthimi
// only, and addon subdirectories build exactly when present there and not
// listed in the configure-time CMake list SCORE_DISABLED_PLUGINS
// (src/addons/CMakeLists.txt:21); there is no runtime plugin-disable switch.
// A read-only clone exists at ~/ossia/.academy but is not built, and its
// OpenPBR-BSDF submodule (the tree carrying the headers) is not even
// initialised there. So the positive half detects availability at runtime --
// can `#include "openpbr.h"` actually resolve against the library this
// machine has? -- and SKIPs with instructions otherwise. To run it green:
// either build with the addon checked out under src/addons/score-addon-academy
// (and not in SCORE_DISABLED_PLUGINS) on a machine where its openpbr package
// is installed into <library>/packages, or point SCORE_TEST_LIBRARY_ROOT at
// any library root whose packages/ tree provides openpbr.h.
//
// Intended registration (tests/gfx/CMakeLists.txt) -- APP mode because the
// resolver reads Library::Settings::Model out of score::AppContext()
// (ShaderProgram.cpp:33), so a booted application is required, exactly like
// test_unit_packagemanager_firstrun (tests/unit/CMakeLists.txt:988-994):
//
//   score_add_test(test_gfx_shader_include_path
//     SOURCES GfxShaderIncludePath.cpp
//     APP
//     PLUGINS score_plugin_gfx score_plugin_library
//     LIBS test_gfx_engine_glue)
//   target_include_directories(test_gfx_shader_include_path PRIVATE
//     "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-library")
//
//   ctest -R gfx_shader_include_path
// =============================================================================

#include <Gfx/Settings/Model.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <Library/LibrarySettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/Addon.hpp>

#include <score_test/App.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
// Restore the library root on the way out however the assertions left the
// stack: a failed REQUIRE unwinds, and the settings model outlives the test
// case inside one app boot.
struct root_path_guard
{
  Library::Settings::Model& lib;
  QString previous{lib.getRootPath()};
  ~root_path_guard() { lib.setRootPath(previous); }
};

struct graphics_api_guard
{
  Gfx::Settings::Model& gfx;
  QString previous{gfx.getGraphicsApi()};
  ~graphics_api_guard() { gfx.setGraphicsApi(previous); }
};

bool write_file(const QString& path, const QByteArray& content)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(content) == content.size();
}

// A well-formed ISF fragment shader whose output colour comes from a function
// only the included header defines: the compile cannot accidentally succeed
// while the include silently fails to resolve.
QString isf_fragment_including(const char* header, const char* fn)
{
  return QStringLiteral("/*{\n"
                        "  \"DESCRIPTION\": \"shader-include-path probe\",\n"
                        "  \"CREDIT\": \"test\",\n"
                        "  \"ISFVSN\": \"2.0\",\n"
                        "  \"CATEGORIES\": [\"TEST\"],\n"
                        "  \"INPUTS\": []\n"
                        "}*/\n"
                        "#include \"%1\"\n"
                        "void main() { gl_FragColor = %2(); }\n")
      .arg(QString::fromUtf8(header), QString::fromUtf8(fn));
}

// The stub every positive leg compiles against.
constexpr const char probe_header_body[]
    = "vec4 gfx_include_probe_color() { return vec4(0.25, 0.5, 0.75, 1.0); }\n";
}

// =============================================================================
// The registration mechanism itself, without needing any addon built: the
// search path is the packages dir plus its first-level subdirectories
// (ShaderProgram.cpp:23-56), re-read on every resolution. Register a temp
// library, resolve; deregister, and the failure names the header and where it
// looked.
//
// Negative control: make the quoted-include miss non-fatal -- i.e. replace the
// `ctx.error = ...; return {};` at ShaderProgram.cpp:318-326 with the
// bracketed branch's verbatim fallthrough (ShaderProgram.cpp:329 onwards) --
// and the "deregistered" half of this case goes red: preprocessShaderIncludes
// would return the unexpanded source with an empty error instead of the
// named diagnostic.
// =============================================================================

TEST_CASE(
    "the shader-include search path follows the library packages dir, live",
    "[gfx][shader][include]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& lib = ctx.settings<Library::Settings::Model>();
    root_path_guard restore{lib};

    // --- Register: a library root whose packages/ dir carries one package
    // subdirectory with a header, and one loose header at the top level.
    QTemporaryDir rootA;
    REQUIRE(rootA.isValid());
    const QString packagesA = rootA.path() + "/packages";
    REQUIRE(QDir{}.mkpath(packagesA + "/probelib"));
    REQUIRE(write_file(packagesA + "/probelib/gfx_include_probe.h", probe_header_body));
    REQUIRE(write_file(
        packagesA + "/gfx_include_direct.h",
        "vec4 gfx_include_direct_color() { return vec4(1.0); }\n"));

    lib.setRootPath(rootA.path());
    REQUIRE(lib.getPackagesPath() == packagesA);

    // A package subdirectory's header resolves by bare name...
    {
      auto [expanded, error]
          = Gfx::preprocessShaderIncludes("#include \"gfx_include_probe.h\"\n");
      INFO(error.toStdString());
      REQUIRE(error.isEmpty());
      CHECK(expanded.contains("gfx_include_probe_color"));
      // ...framed with a #line marker naming the header
      // (ShaderProgram.cpp:246), so downstream glslang errors are
      // attributable to the included file.
      CHECK(expanded.contains("#line 1 \""));
      CHECK(expanded.contains("gfx_include_probe.h"));
      // The directive line itself must be gone -- replaced, not merely
      // prefixed.
      CHECK(!expanded.contains("#include"));
    }

    // ...and so does a header sitting directly in packages/.
    {
      auto [expanded, error]
          = Gfx::preprocessShaderIncludes("#include \"gfx_include_direct.h\"\n");
      INFO(error.toStdString());
      REQUIRE(error.isEmpty());
      CHECK(expanded.contains("gfx_include_direct_color"));
    }

    // --- Deregister: point the library at a root that has a packages/ dir
    // but no such header. The same resolution must now fail, and the
    // diagnostic must name BOTH the missing header and the directories that
    // were searched -- and only those: the old root must no longer appear,
    // proving the path list is recomputed per call rather than latched at
    // startup (shaderIncludePaths() is called anew inside
    // preprocessShaderIncludes, ShaderProgram.cpp:665).
    QTemporaryDir rootB;
    REQUIRE(rootB.isValid());
    const QString packagesB = rootB.path() + "/packages";
    REQUIRE(QDir{}.mkpath(packagesB));
    lib.setRootPath(rootB.path());

    {
      auto [expanded, error]
          = Gfx::preprocessShaderIncludes("#include \"gfx_include_probe.h\"\n");
      REQUIRE(!error.isEmpty());
      CHECK(expanded.isEmpty());
      CHECK(error.contains(
          QStringLiteral("Shader include not found: \"gfx_include_probe.h\"")));
      CHECK(error.contains(QStringLiteral("searched:")));
      CHECK(error.contains(packagesB));
      CHECK(!error.contains(rootA.path()));
    }
  });
}

// =============================================================================
// P1-16 proper, negative then positive, through the pipeline the lgm scores
// actually use: ProgramCache::instance().get().
//
// Without any openpbr provider the compile result is a named diagnostic --
// "Fragment: Shader include not found: \"openpbr.h\" (searched: ...)" -- and
// std::nullopt, never a silently-empty program (a black frame). With a
// package providing the header, the very same ShaderSource compiles through
// QShaderBaker to a ProcessedProgram, which also proves the earlier failure
// was not cached (ProgramCache stores only successes, ShaderProgram.cpp:561).
//
// Negative control: cache the failure too -- hoist `programs[cacheKey] = ...`
// above the include resolution, or store the nullopt result at
// ShaderProgram.cpp:482 -- and the second half of this case goes red: the
// recompile after installing the package would replay the stale miss.
// =============================================================================

TEST_CASE(
    "a shader including openpbr.h fails diagnosably without the academy "
    "include path, and compiles once a package provides it",
    "[gfx][shader][include][openpbr]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& lib = ctx.settings<Library::Settings::Model>();
    auto& gfx = ctx.settings<Gfx::Settings::Model>();
    root_path_guard restore_root{lib};
    graphics_api_guard restore_api{gfx};

    // Pin the baker target to Vulkan: shaderVersionForAPI(Vulkan) is the
    // constant QShaderVersion(100) (Gfx/Settings/Model.cpp:269-271), whereas
    // the OpenGL branch instantiates GLCapabilities, which wants a GL
    // context this offscreen test does not have (Model.cpp:266-267).
    // QShaderBaker emits SPIR-V on any host; no device is touched.
    gfx.setGraphicsApi(Gfx::Settings::GraphicsApis{}.Vulkan);

    // Hermetic library root with an empty packages/ dir: guarantees
    // openpbr.h is NOT resolvable here even on a machine whose real user
    // library has the academy package installed.
    QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString packages = root.path() + "/packages";
    REQUIRE(QDir{}.mkpath(packages));
    lib.setRootPath(root.path());

    const Gfx::ShaderSource source{
        Gfx::ShaderSource::ProgramType::ISF, QString{},
        isf_fragment_including("openpbr.h", "openpbr_probe_color")};

    // --- Without the include path: DIAGNOSABLE failure, not a black frame.
    {
      auto [program, error] = Gfx::ProgramCache::instance().get(source, QString{});
      REQUIRE(!program.has_value());
      REQUIRE(!error.isEmpty());
      // Stage prefix from ProgramCache::get (ShaderProgram.cpp:482)...
      CHECK(error.startsWith(QStringLiteral("Fragment: ")));
      // ...the fatal quoted-include miss names the header
      // (ShaderProgram.cpp:319)...
      CHECK(error.contains(
          QStringLiteral("Shader include not found: \"openpbr.h\"")));
      // ...and where it looked, so the user can see the academy package is
      // simply not installed there.
      CHECK(error.contains(QStringLiteral("searched:")));
      CHECK(error.contains(packages));
    }

    // --- Install a package providing the header ("registering" the include
    // path the way score-addon-academy's environment does: a package dir
    // under packages/, picked up by shaderIncludePaths()'s first-level-subdir
    // scan, ShaderProgram.cpp:42-53) and recompile the SAME source.
    REQUIRE(QDir{}.mkpath(packages + "/openpbr"));
    REQUIRE(write_file(
        packages + "/openpbr/openpbr.h",
        "vec4 openpbr_probe_color() { return vec4(0.25, 0.5, 0.75, 1.0); }\n"));

    {
      auto [program, error] = Gfx::ProgramCache::instance().get(source, QString{});
      INFO(error.toStdString());
      REQUIRE(error.isEmpty());
      REQUIRE(program.has_value());
      // The processed program really absorbed the header: the function the
      // shader calls is present in the fragment stage that was baked.
      CHECK(program->fragment.contains(
          QStringLiteral("openpbr_probe_color")));
    }
  });
}

// =============================================================================
// The positive half of P1-16 against the REAL openpbr.h, on machines that
// have it. This checkout does not build score-addon-academy (src/addons/
// carries no such directory; addons build when present there and absent from
// the configure-time SCORE_DISABLED_PLUGINS list, src/addons/CMakeLists.txt:21),
// so availability is detected at runtime: either the default library that the
// booted app resolved (LibrarySettings.cpp:127-137) provides the header, or
// the runner points SCORE_TEST_LIBRARY_ROOT at a library root whose
// packages/ tree does. Otherwise: SKIP, with the enablement instructions in
// the message.
// =============================================================================

TEST_CASE(
    "openpbr.h resolves through the registered include path when the academy "
    "library is installed",
    "[gfx][shader][include][openpbr][academy]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& lib = ctx.settings<Library::Settings::Model>();
    root_path_guard restore{lib};

    if(const auto env = qEnvironmentVariable("SCORE_TEST_LIBRARY_ROOT");
       !env.isEmpty())
      lib.setRootPath(env);

    // Purely informational: whether any loaded addon calls itself academy.
    // The include path is a property of the installed library, not of the
    // plugin binary, so this is context for the log rather than the gate.
    bool academy_addon_loaded = false;
    for(const score::Addon& addon : ctx.addons())
    {
      if(addon.name.contains(QStringLiteral("academy"), Qt::CaseInsensitive)
         || addon.path.contains(QStringLiteral("academy"), Qt::CaseInsensitive))
        academy_addon_loaded = true;
    }
    INFO(
        "academy addon loaded: " << (academy_addon_loaded ? "yes" : "no")
                                 << "; library root: "
                                 << lib.getRootPath().toStdString());

    auto [expanded, error]
        = Gfx::preprocessShaderIncludes("#include \"openpbr.h\"\n");
    if(!error.isEmpty())
    {
      SKIP(
          "openpbr.h does not resolve against this machine's library ("
          + error.toStdString()
          + "). To run this half: build with score-addon-academy checked out "
            "under src/addons/ (and not in SCORE_DISABLED_PLUGINS) on a "
            "machine where its openpbr package is installed into "
            "<library>/packages, or set SCORE_TEST_LIBRARY_ROOT to a library "
            "root whose packages/ tree provides openpbr.h.");
    }

    // The header resolved through the registered path: the expansion is
    // attributable (framed by the #line marker naming the real file,
    // ShaderProgram.cpp:246) and non-trivial.
    CHECK(expanded.contains("#line 1 \""));
    CHECK(expanded.contains("openpbr"));
    CHECK(expanded.size() > int(sizeof("#include \"openpbr.h\"\n")));
  });
}
