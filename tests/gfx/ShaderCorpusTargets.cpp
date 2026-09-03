// =============================================================================
// Every shader in tests/gfx/corpus, baked for every backend's shading language.
//
// WHY THIS EXISTS
//
// 0411ad04bb fixed a defect that had been in the tree for as long as raw-raster
// shaders have had IMG_* accessors: parse_raw_raster_pipeline() emitted
// `#define isf_FragCoord` twice, once through GLSL45.defaultFunctions (guarded
// on QSHADER_SPIRV alone) and once of its own (guarded on QSHADER_SPIRV ||
// QSHADER_HLSL || QSHADER_MSL). ShaderCache sets setPerTargetCompilation(true),
// so glslang preprocesses the source once per target with that target's
// QSHADER_* macro defined:
//
//   SPIR-V   both branches take the flipped form  -> identical text, accepted
//   GLSL     both branches take gl_FragCoord      -> identical text, accepted
//   HLSL/MSL one branch each way                  -> different text, REJECTED
//
//     ERROR: :123: '#define' : Macro redefined; different substitutions:
//            isf_FragCoord
//
// So NO raw-raster shader had ever compiled on Direct3D or Metal, and the only
// thing that could notice was a four-backend Windows sweep. The two backends
// Linux CI runs -- OpenGL and Vulkan -- are exactly the two the defect spared.
//
// The fix is one hunk. Nothing stopped the next one, because nothing in an
// ordinary ctest run ever asked glslang for HLSL or MSL.
//
// NEGATIVE CONTROL (this machine, Qt 6.13, 2026-09-03)
//
// With 0411ad04bb reverted -- `git revert -n 0411ad04bb`, its single isf.cpp
// hunk, rebuilt -- this case goes RED:
//
//   RAW_RASTER_PIPELINE: 24 files, 48 stages, 160 bakes ok (was 232)
//   GLSL:   179 clean  (unchanged)
//   SPIR-V: 179 clean  (unchanged)
//   MSL:    155 clean  (was 179) -- 24 fragment stages lost
//   HLSL:   300 clean  (was 348) -- 48, the same 24 on both D3D backends
//
//   RAW_RASTER_PIPELINE raw-raster-basic.fs [fragment] on Metal (MSL):
//     ERROR: :131: '#define' : Macro redefined; different substitutions:
//            isf_FragCoord
//     ERROR: :132: '' : missing #endif
//     ERROR: 2 compilation errors.  No code generated.
//
// 72 (stage, target) failures, all 24 raw-raster fragment stages, on exactly
// the two languages the defect lived behind and on neither of the other two --
// which is the shape the fix's own message describes. Restored: green again.
//
// WHAT MAKES THIS CHEAP
//
// score::gfx::ShaderCache::get(api, version, source, stage, multiViewCount) is
// PURE SHADER COMPILATION -- QShaderBaker and nothing else. No device, no
// display, no GPU. QShaderBaker cross-compiles to HLSL and MSL on any host, so
// the D3D and Metal legs of this file run on the same Linux box that can only
// bring up OpenGL and Vulkan. Same no-device pattern as GfxCsfOrientGate.cpp.
// The pair's second member is the baker's error message; empty means success.
//
// WHAT IT COVERS
//
// The corpus is not one kind of shader. Each kind reaches the baker through a
// different generation path, and this file drives each through the path the
// APPLICATION uses, not a reimplementation of it:
//
//   ISF (.fs, no MODE header)
//       Gfx::programFromISFFragmentShaderPath -> ProgramCache::get. The
//       ProcessedProgram's vertex/fragment ARE what score::gfx::ISFNode is
//       constructed with and what RenderedISFNode later bakes.
//
//   RAW_RASTER_PIPELINE (.fs/.frag + sibling .vs/.vert)
//       ShaderSource{RawRasterPipeline, vert, frag} -> ProgramCache::get,
//       byte for byte what Gfx::RenderPipeline::Model::setProgram and the
//       test fixture's make_raster_node (tests/fixtures/score_test/Gfx.hpp:388)
//       do. THIS IS THE CASE THAT WAS BROKEN; it is covered from the generated
//       text, not skipped.
//
//   VERTEX_SHADER_ART (.vs, no .fs sibling)
//       ShaderSource{VertexShaderArt, vs, {}} -> ProgramCache::get.
//
//   COMPUTE_SHADER (.cs)
//       isf::parser in CSF mode -> compute_shader(), with the ISF_LOCAL_SIZE_*
//       placeholders substituted the way RenderedCSFNode does (the baker
//       rejects them). CSF does not go through ProgramCache -- Gfx::CSF::Model
//       parses inline -- and neither does this.
//
//   GEOMETRY_FILTER (.glsl)
//       isf::parser in GeometryFilter mode -> geometry_filter(), spliced into
//       a host vertex stage. See the host-template note below.
//
// and asserts that ProgramCache/the parser produced something, and that EVERY
// generated stage bakes clean for all five backends: GLSL (OpenGL), SPIR-V
// (Vulkan), HLSL (D3D11 and D3D12) and MSL (Metal), each at the shader version
// Gfx::Settings::shaderVersionForAPI hands that backend.
//
// NOTHING IS SKIPPED SILENTLY. Every corpus file is classified; the
// classification is asserted to account for all of them; every classified file
// is asserted to have been either generated or skipped with a stated reason;
// and an accounting identity asserts that every generated stage was offered to
// every target and that each offer came back as exactly one of {clean, known
// gap, failure}. A sweep over an empty set produces the same empty failure list
// as a sweep that works, so the counts are the part that says which one this
// was. Measured on this tree, 2026-09-03, Qt 6.13: 179 generated stages, 179
// clean GLSL / 179 SPIR-V / 179 MSL / 348 HLSL (two D3D backends) + 10 known
// gaps.
//
// KNOWN GAPS, and what this file does with them
//
// Three real limitations exist that are NOT defects in this corpus. Each is
// declared in `known_gaps` / handled by an explicit rule below, and each is
// reported BY NAME in the run's output with which way it went on this host --
// they are properties of the SPIRV-Cross inside the host's qtshadertools and
// they do not answer the same way on every Qt, so tolerating either outcome
// and printing it is the honest handling. What is NOT tolerated is a gap
// turning into "not baked at all": the accounting identity at the bottom of
// the case counts every (stage, target) offer, and a gap has to be one of
// {clean here, still fails here}.
//
//   1. RWTextureCube does not exist in HLSL. SPIRV-Cross cannot express a
//      write-only/read-write cube storage image in any shader model; it fails
//      identically at SM 5.0 and SM 6.1.
//   2. corpus/isf-long-numeric.fs declares a variable named `frac`, and
//      SPIRV-Cross renames GLSL fract() to HLSL frac() without renaming the
//      user's variable, so it emits `float frac = frac(...)`.
//   3. gl_ViewIndex needs SV_ViewID, which needs Shader Model >= 6.1, while
//      Gfx/Settings/Model.cpp:278 asks QShaderBaker for QShaderVersion(50).
//      Handled as a RULE rather than a file list (it follows from the
//      descriptor's multiview_count): a multiview shader that fails at SM 5.0
//      is re-baked at SM 6.1, and only counts as a known gap if the SM 6.1
//      bake succeeds -- which is what separates "the version request is too
//      low" from "the shader is broken". A multiview shader that fails at BOTH
//      is a plain failure, which is how the isf_FragCoord defect would still be
//      caught in the multiview raw-rasters.
//
// A fourth, version-shaped one: QShaderBaker::setMultiViewCount arrived in Qt
// 6.7 (ShaderCache.cpp:92-95 guards it). CI compiles the tests against distro
// Qt 6.4.2, where ShaderCache cannot emit `layout(num_views = N)` at all and
// every multiview shader fails for every target. On such a Qt the multiview
// shaders SKIP, by name and with that reason, instead of failing.
//
// GPU-less, so this is a plain (non-GUI) target -- but APP mode, because
// ProgramCache::get reads Gfx::Settings::Model out of score::AppContext()
// (ShaderProgram.cpp:534). The settings API is pinned to Vulkan for the same
// reason GfxShaderIncludePath.cpp pins it: shaderVersionForAPI(OpenGL)
// constructs score::GLCapabilities{}, which wants a GL context.
//
//   ctest -R gfx_shader_corpus_targets
// =============================================================================

#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <score_test/App.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace
{
// -----------------------------------------------------------------------------
// The five backends, and the shading language each one's bake produces.
// -----------------------------------------------------------------------------
enum class Lang
{
  Glsl,
  SpirV,
  Hlsl,
  Msl
};

const char* lang_name(Lang l) noexcept
{
  switch(l)
  {
    case Lang::Glsl:
      return "GLSL";
    case Lang::SpirV:
      return "SPIR-V";
    case Lang::Hlsl:
      return "HLSL";
    case Lang::Msl:
      return "MSL";
  }
  return "?";
}

struct Target
{
  score::gfx::GraphicsApi api;
  const char* backend;
  Lang lang;
  int version;
};

// The version each backend is baked at, as Gfx::Settings::shaderVersionForAPI()
// hands them out (Gfx/Settings/Model.cpp:262-284). Spelled out rather than
// called, because the OpenGL answer there comes from GLCapabilities, which
// wants a context; nothing in this file has one. Same choice as
// GfxCsfOrientGate.cpp.
const Target all_targets[] = {
    {score::gfx::OpenGL, "OpenGL", Lang::Glsl, 450},
    {score::gfx::Vulkan, "Vulkan", Lang::SpirV, 100},
    {score::gfx::D3D11, "Direct3D 11", Lang::Hlsl, 50},
    {score::gfx::D3D12, "Direct3D 12", Lang::Hlsl, 50},
    {score::gfx::Metal, "Metal", Lang::Msl, 12},
};

// Shader Model 6.1 -- the first that has SV_ViewID. Only used to tell "the
// version score asks for is too low" apart from "this shader does not compile".
constexpr int hlsl_sm61 = 61;

// -----------------------------------------------------------------------------
// Corpus classification.
// -----------------------------------------------------------------------------
enum class Kind
{
  Isf,
  RawRaster,
  Vsa,
  Csf,
  GeometryFilter,
  // A .vs/.vert that is the sibling of a .fs/.frag: it is not a shader of its
  // own, it is half of the pair the fragment file names. Baked with that pair.
  VertexCompanion,
  Unclassified
};

const char* kind_name(Kind k) noexcept
{
  switch(k)
  {
    case Kind::Isf:
      return "ISF";
    case Kind::RawRaster:
      return "RAW_RASTER_PIPELINE";
    case Kind::Vsa:
      return "VERTEX_SHADER_ART";
    case Kind::Csf:
      return "COMPUTE_SHADER";
    case Kind::GeometryFilter:
      return "GEOMETRY_FILTER";
    case Kind::VertexCompanion:
      return "vertex companion";
    case Kind::Unclassified:
      return "UNCLASSIFIED";
  }
  return "?";
}

QByteArray read_all(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll();
}

/// The sibling vertex file of a fragment file, following the same two naming
/// conventions Gfx::programFromISFFragmentShaderPath tries
/// (ShaderProgram.cpp:597-601). Empty if there is none on disk.
QString vertex_sibling(const QString& fsPath)
{
  const QString candidates[] = {
      QString{fsPath}.replace(".frag", ".vert").replace(".fs", ".vs"),
      QString{fsPath}.replace(".frag", ".vs"),
      QString{fsPath}.replace(".fs", ".vert"),
  };
  for(const QString& c : candidates)
  {
    if(c == fsPath)
      continue;
    if(QFile::exists(c))
      return c;
  }
  return {};
}

/// The sibling fragment file of a vertex file, i.e. vertex_sibling() read
/// backwards. Empty if there is none on disk.
QString fragment_sibling(const QString& vsPath)
{
  const QString candidates[] = {
      QString{vsPath}.replace(".vert", ".frag").replace(".vs", ".fs"),
      QString{vsPath}.replace(".vert", ".fs"),
      QString{vsPath}.replace(".vs", ".frag"),
  };
  for(const QString& c : candidates)
  {
    if(c == vsPath)
      continue;
    if(QFile::exists(c))
      return c;
  }
  return {};
}

/// The MODE the ISF header declares, or an empty string. Deliberately textual:
/// the point is to route the file to the generation path the application would
/// route it to, and the application's own router (isf::parser) is what is under
/// test downstream.
QByteArray declared_mode(const QByteArray& text)
{
  const int at = text.indexOf("\"MODE\"");
  if(at < 0)
    return {};
  const int colon = text.indexOf(':', at);
  if(colon < 0)
    return {};
  const int open = text.indexOf('"', colon);
  if(open < 0)
    return {};
  const int close = text.indexOf('"', open + 1);
  if(close < 0)
    return {};
  return text.mid(open + 1, close - open - 1);
}

struct CorpusFile
{
  QString path;
  QString name;
  Kind kind{Kind::Unclassified};
  QString companion; // for RawRaster: the .vs/.vert half
};

Kind classify(const QString& path, const QString& name, QString& companion)
{
  const QString suffix = QFileInfo{name}.suffix().toLower();
  if(suffix == QStringLiteral("cs"))
    return Kind::Csf;
  if(suffix == QStringLiteral("glsl"))
    return Kind::GeometryFilter;

  const QByteArray text = read_all(path);
  const QByteArray mode = declared_mode(text);

  if(suffix == QStringLiteral("fs") || suffix == QStringLiteral("frag"))
  {
    if(mode == "RAW_RASTER_PIPELINE")
    {
      companion = vertex_sibling(path);
      return Kind::RawRaster;
    }
    // Every other fragment file in the corpus is a fullscreen ISF, whether or
    // not it declares a MODE (ISF is the parser's default and what the Filter
    // process passes).
    return Kind::Isf;
  }

  if(suffix == QStringLiteral("vs") || suffix == QStringLiteral("vert"))
  {
    // A vertex file that a fragment file names is baked as part of that pair.
    if(!fragment_sibling(path).isEmpty())
      return Kind::VertexCompanion;
    if(mode == "VERTEX_SHADER_ART")
      return Kind::Vsa;
  }

  return Kind::Unclassified;
}

/// Does the shader -- or the vertex half of its pair -- declare "MULTIVIEW" in
/// its ISF header? Read off the text rather than off descriptor.multiview_count
/// because the caller needs the answer BEFORE generation: on a Qt older than
/// 6.7, generation of a multiview shader is itself what fails.
bool declares_multiview(const CorpusFile& f)
{
  if(read_all(f.path).contains("\"MULTIVIEW\""))
    return true;
  if(!f.companion.isEmpty() && read_all(f.companion).contains("\"MULTIVIEW\""))
    return true;
  return false;
}

// -----------------------------------------------------------------------------
// Generation: corpus file -> the GLSL stages the engine would hand the baker.
// -----------------------------------------------------------------------------
struct Stage
{
  QShader::Stage stage;
  const char* name;
  QByteArray source;
};

struct Generated
{
  std::vector<Stage> stages;
  int multiview{0};
  QString error; // non-empty: generation itself failed, before any bake
};

Generated generate_isf(const QString& path)
{
  Generated g;
  Gfx::ShaderSource src = Gfx::programFromISFFragmentShaderPath(path, {});
  auto [processed, err] = Gfx::ProgramCache::instance().get(src, path);
  if(!err.isEmpty())
  {
    g.error = err;
    return g;
  }
  if(!processed)
  {
    g.error = QStringLiteral("ProgramCache produced no program and no error");
    return g;
  }
  g.multiview = processed->descriptor.multiview_count;
  g.stages.push_back(
      {QShader::VertexStage, "vertex", processed->vertex.toUtf8()});
  g.stages.push_back(
      {QShader::FragmentStage, "fragment", processed->fragment.toUtf8()});
  return g;
}

Generated generate_raw_raster(const QString& vsPath, const QString& fsPath)
{
  Generated g;
  if(vsPath.isEmpty())
  {
    g.error = QStringLiteral("no sibling vertex shader on disk");
    return g;
  }

  Gfx::ShaderSource src{
      Gfx::ShaderSource::ProgramType::RawRasterPipeline,
      QString::fromUtf8(read_all(vsPath)), QString::fromUtf8(read_all(fsPath))};

  auto [processed, err] = Gfx::ProgramCache::instance().get(src, fsPath);
  if(!err.isEmpty())
  {
    g.error = err;
    return g;
  }
  if(!processed)
  {
    g.error = QStringLiteral("ProgramCache produced no program and no error");
    return g;
  }
  if(processed->descriptor.mode != isf::descriptor::RawRaster)
  {
    g.error = QStringLiteral("did not parse as RAW_RASTER_PIPELINE");
    return g;
  }
  g.multiview = processed->descriptor.multiview_count;
  g.stages.push_back(
      {QShader::VertexStage, "vertex", processed->vertex.toUtf8()});
  g.stages.push_back(
      {QShader::FragmentStage, "fragment", processed->fragment.toUtf8()});
  return g;
}

Generated generate_vsa(const QString& vsPath)
{
  Generated g;
  Gfx::ShaderSource src{
      Gfx::ShaderSource::ProgramType::VertexShaderArt,
      QString::fromUtf8(read_all(vsPath)), QString{}};

  auto [processed, err] = Gfx::ProgramCache::instance().get(src, vsPath);
  if(!err.isEmpty())
  {
    g.error = err;
    return g;
  }
  if(!processed)
  {
    g.error = QStringLiteral("ProgramCache produced no program and no error");
    return g;
  }
  g.multiview = processed->descriptor.multiview_count;
  g.stages.push_back(
      {QShader::VertexStage, "vertex", processed->vertex.toUtf8()});
  g.stages.push_back(
      {QShader::FragmentStage, "fragment", processed->fragment.toUtf8()});
  return g;
}

Generated generate_csf(const QString& path)
{
  Generated g;
  auto [resolved, incErr] = Gfx::preprocessShaderIncludes(read_all(path), path);
  if(!incErr.isEmpty())
  {
    g.error = QStringLiteral("#include: ") + incErr;
    return g;
  }

  try
  {
    isf::parser p{
        std::string(resolved.constData(), std::size_t(resolved.size())),
        isf::parser::ShaderType::CSF};
    if(p.mode() != isf::descriptor::CSF)
    {
      g.error = QStringLiteral("did not parse as COMPUTE_SHADER");
      return g;
    }
    const isf::descriptor desc = p.data();
    if(desc.csf_passes.empty())
    {
      g.error = QStringLiteral("declares no PASSES");
      return g;
    }

    // RenderedCSFNode substitutes the work-group size before baking; the
    // placeholders are not GLSL and the baker rejects them.
    const auto& ls = desc.csf_passes[0].local_size;
    QString src = QString::fromStdString(p.compute_shader());
    src.replace(QStringLiteral("ISF_LOCAL_SIZE_X"), QString::number(ls[0]));
    src.replace(QStringLiteral("ISF_LOCAL_SIZE_Y"), QString::number(ls[1]));
    src.replace(QStringLiteral("ISF_LOCAL_SIZE_Z"), QString::number(ls[2]));

    g.multiview = desc.multiview_count;
    g.stages.push_back({QShader::ComputeStage, "compute", src.toUtf8()});
  }
  catch(const std::exception& e)
  {
    g.error = QStringLiteral("CSF parse error: ") + QString::fromUtf8(e.what());
  }
  catch(...)
  {
    g.error = QStringLiteral("unknown CSF parse error");
  }
  return g;
}

// A geometry filter is a SNIPPET, not a stage: isf::parser::geometry_filter()
// returns `process_vertex_%node%(...)` plus a `filter_%node%` UBO, which a HOST
// vertex shader splices in at %vtx_define_filters%. There are two hosts in the
// tree -- Gfx/GeometryFilter/Process.cpp Model::validate() (the compile probe
// the editor runs) and the eight ModelDisplayNode.cpp variants -- and BOTH are
// file-static strings inside their translation units, reachable from nothing.
//
// So this host is a copy of Model::validate()'s (Process.cpp:50-104), and it
// carries that copy's risk: if the product's template gains a construct this
// one does not have, this file bakes the older shape. That is a narrower gap
// than not baking the geometry filter at all, which is the only alternative
// without exporting one of the two. The splice is asserted to have happened
// (the composed text must name the filter function), so a parser change that
// stops emitting one is not silently baked as an empty host.
constexpr auto geometry_filter_host = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 1) uniform process_t {
  float TIME;
  float TIMEDELTA;
  float PROGRESS;
  float SAMPLERATE;

  int PASSINDEX;
  int FRAMEINDEX;

  vec2 RENDERSIZE;
  vec4 DATE;
} isf_process_uniforms;

layout(std140, binding = 2) uniform material_t {
  mat4 matrixModelViewProjection;
  mat4 matrixModelView;
  mat4 matrixModel;
  mat4 matrixView;
  mat4 matrixProjection;
  mat3 matrixNormal;
} mat;

float TIME = isf_process_uniforms.TIME;
float TIMEDELTA = isf_process_uniforms.TIMEDELTA;
float PROGRESS = isf_process_uniforms.PROGRESS;
int PASSINDEX = isf_process_uniforms.PASSINDEX;
int FRAMEINDEX = isf_process_uniforms.FRAMEINDEX;
vec4 DATE = isf_process_uniforms.DATE;

%vtx_define_filters%

out gl_PerVertex {
vec4 gl_Position;
};

void main()
{
  vec3 in_position = vec3(0);
  vec3 in_normal = vec3(0);
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  process_vertex_0(in_position, in_normal, in_uv, in_tangent, in_color);

  gl_Position.xyz = in_position;
}
)_";

Generated generate_geometry_filter(const QString& path)
{
  Generated g;
  try
  {
    isf::parser p{
        std::string{read_all(path).toStdString()},
        isf::parser::ShaderType::GeometryFilter};

    QString snippet = QString::fromStdString(p.geometry_filter());
    if(snippet.isEmpty())
    {
      g.error = QStringLiteral("geometry_filter() produced nothing");
      return g;
    }
    // The two substitutions Process.cpp:116-117 makes for the compile probe.
    snippet.replace(QStringLiteral("%next%"), QStringLiteral("4"));
    snippet.replace(QStringLiteral("%node%"), QStringLiteral("0"));

    QString vtx = QString::fromUtf8(geometry_filter_host);
    vtx.replace(QStringLiteral("%vtx_define_filters%"), snippet);
    if(!vtx.contains(QStringLiteral("process_vertex_0(inout")))
    {
      g.error = QStringLiteral(
          "the composed host does not declare process_vertex_0 -- the splice "
          "did not take, so baking it would prove nothing");
      return g;
    }

    g.stages.push_back({QShader::VertexStage, "vertex", vtx.toUtf8()});
  }
  catch(const std::exception& e)
  {
    g.error = QStringLiteral("geometry-filter parse error: ")
              + QString::fromUtf8(e.what());
  }
  catch(...)
  {
    g.error = QStringLiteral("unknown geometry-filter parse error");
  }
  return g;
}

// -----------------------------------------------------------------------------
// Known gaps: real limitations of SPIRV-Cross / of the requested shader model
// that are NOT defects in these shaders.
//
// A gap is TOLERATED, not asserted-to-fail. Both entries below are properties
// of the SPIRV-Cross that ships inside the host's qtshadertools, and they do
// not answer the same way on every Qt: measured here on Qt 6.13, the `frac`
// one no longer reproduces while the RWTextureCube one still does. Pinning a
// gap as a hard expected-failure would therefore turn the test red on the very
// Qt where the gap was fixed, which is the opposite of useful. So each outcome
// is COUNTED and REPORTED by name -- "known gap, still fails" or "known gap,
// no longer reproduces here" -- and the accounting identity at the bottom
// makes sure neither outcome can quietly become "not baked at all".
// -----------------------------------------------------------------------------
struct KnownGap
{
  const char* file;
  Lang lang;
  const char* why;
};

const KnownGap known_gaps[] = {
    {"csf-cube-image-write.cs", Lang::Hlsl,
     "RWTextureCube does not exist in HLSL: SPIRV-Cross cannot express a "
     "writable cube storage image in any shader model (fails identically at "
     "SM 5.0 and SM 6.1)"},
    {"isf-long-numeric.fs", Lang::Hlsl,
     "the shader declares `float frac` (isf-long-numeric.fs:15) and older "
     "SPIRV-Cross renames GLSL fract() to HLSL frac() without renaming the "
     "user's variable, emitting `float frac = frac(...)`"},
};

const KnownGap* find_gap(const QString& file, Lang lang)
{
  for(const auto& g : known_gaps)
    if(file == QString::fromLatin1(g.file) && g.lang == lang)
      return &g;
  return nullptr;
}

// -----------------------------------------------------------------------------
// The sweep.
// -----------------------------------------------------------------------------
struct Tally
{
  int files{0};     // corpus files of this kind
  int generated{0}; // ...whose stages the engine produced
  int skipped{0};   // ...skipped before generation, with a stated reason
  int stages{0};    // generated stages
  int bakes_ok{0};  // (stage, target) pairs that baked clean
  int gaps{0};      // (stage, target) pairs that failed as a known gap
};

struct Sweep
{
  std::map<Kind, Tally> per_kind;
  std::map<std::string, int> bakes_ok_per_lang;
  std::map<std::string, int> gaps_per_lang;
  int stages_total{0};
  std::vector<std::string> failures;
  std::vector<std::string> notes; // gaps and skips, one line each
};

/// Bake one stage for one target. Returns the baker's error message.
QString bake(const Target& t, const Stage& s, int multiview, int version)
{
  const auto& [shader, error] = score::gfx::ShaderCache::get(
      t.api, QShaderVersion(version), s.source, s.stage, multiview);
  if(!error.isEmpty())
    return error;
  if(!shader.isValid())
    return QStringLiteral("baker reported no error but produced no shader");
  return {};
}

void sweep_one(Sweep& sw, const CorpusFile& f, const Generated& g)
{
  Tally& tally = sw.per_kind[f.kind];

  if(!g.error.isEmpty())
  {
    sw.failures.push_back(
        std::string{kind_name(f.kind)} + " " + f.name.toStdString()
        + ": GENERATION FAILED: " + g.error.toStdString());
    return;
  }
  if(g.stages.empty())
  {
    sw.failures.push_back(
        std::string{kind_name(f.kind)} + " " + f.name.toStdString()
        + ": generation produced no stages at all");
    return;
  }

  tally.generated++;
  tally.stages += int(g.stages.size());
  sw.stages_total += int(g.stages.size());

  for(const Target& t : all_targets)
  {
    for(const Stage& s : g.stages)
    {
      const QString err = bake(t, s, g.multiview, t.version);
      const std::string where = std::string{kind_name(f.kind)} + " "
                                + f.name.toStdString() + " [" + s.name + "] on "
                                + t.backend + " (" + lang_name(t.lang) + ")";

      if(const KnownGap* gap = find_gap(f.name, t.lang))
      {
        if(err.isEmpty())
        {
          tally.bakes_ok++;
          sw.bakes_ok_per_lang[lang_name(t.lang)]++;
          sw.notes.push_back(
              "KNOWN GAP HEALED HERE " + where
              + ": bakes clean on this host's qtshadertools, so the entry is "
                "kept only for the older SPIRV-Cross CI builds against ("
              + gap->why + ")");
        }
        else
        {
          tally.gaps++;
          sw.gaps_per_lang[lang_name(t.lang)]++;
          sw.notes.push_back("KNOWN GAP " + where + ": " + gap->why);
        }
        continue;
      }

      if(err.isEmpty())
      {
        tally.bakes_ok++;
        sw.bakes_ok_per_lang[lang_name(t.lang)]++;
        continue;
      }

      // SV_ViewID needs SM >= 6.1, and Settings/Model.cpp:278 asks for
      // QShaderVersion(50). Re-bake at 6.1: only a shader that compiles THERE
      // is failing because of the version request rather than because of its
      // own text.
      if(t.lang == Lang::Hlsl && g.multiview >= 2)
      {
        const QString at61 = bake(t, s, g.multiview, hlsl_sm61);
        if(at61.isEmpty())
        {
          tally.gaps++;
          sw.gaps_per_lang[lang_name(t.lang)]++;
          sw.notes.push_back(
              "KNOWN GAP " + where
              + ": gl_ViewIndex needs SV_ViewID (Shader Model >= 6.1) while "
                "Gfx/Settings/Model.cpp:278 requests QShaderVersion(50); the "
                "same stage bakes clean at SM 6.1");
          continue;
        }
        sw.failures.push_back(
            where + ": fails at SM 5.0 AND at SM 6.1, so this is the shader, "
                    "not the version request.\n    SM 5.0: "
            + err.toStdString() + "\n    SM 6.1: " + at61.toStdString());
        continue;
      }

      sw.failures.push_back(where + ":\n    " + err.toStdString());
    }
  }
}

std::string join(const std::vector<std::string>& v)
{
  std::string out;
  for(const auto& s : v)
    out += "\n  - " + s;
  return out;
}
}

TEST_CASE(
    "every corpus shader bakes for GLSL, SPIR-V, HLSL and MSL",
    "[gfx][l1][shader][corpus][hlsl][msl][portability]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& gfx = ctx.settings<Gfx::Settings::Model>();
    const QString previous_api = gfx.getGraphicsApi();

    // ProgramCache::get bakes for the SETTINGS api on its way to producing the
    // ProcessedProgram (ShaderProgram.cpp:534-557). Pin it to Vulkan: that
    // branch of shaderVersionForAPI is the constant QShaderVersion(100), while
    // the OpenGL branch instantiates GLCapabilities and wants a GL context this
    // test does not have. QShaderBaker emits SPIR-V on any host; no device is
    // touched. Same pin, same reason, as GfxShaderIncludePath.cpp:275.
    gfx.setGraphicsApi(Gfx::Settings::GraphicsApis{}.Vulkan);
    struct restore
    {
      Gfx::Settings::Model& m;
      QString v;
      ~restore() { m.setGraphicsApi(v); }
    } restore_api{gfx, previous_api};

    // ---- 1. enumerate and classify -----------------------------------------
    QDir dir{QStringLiteral(GFX_TEST_CORPUS_DIR)};
    REQUIRE(dir.exists());

    const QStringList names = dir.entryList(QDir::Files, QDir::Name);
    REQUIRE(!names.isEmpty());

    std::vector<CorpusFile> files;
    files.reserve(std::size_t(names.size()));
    for(const QString& n : names)
    {
      CorpusFile f;
      f.name = n;
      f.path = dir.absoluteFilePath(n);
      f.kind = classify(f.path, n, f.companion);
      files.push_back(std::move(f));
    }

    // Nothing in the corpus may be unaccounted for: an unrecognised file is a
    // shader nobody bakes, which is the state this whole test exists to end.
    std::vector<std::string> unclassified;
    for(const auto& f : files)
      if(f.kind == Kind::Unclassified)
        unclassified.push_back(f.name.toStdString());

    INFO("unclassified corpus files:" << join(unclassified));
    CHECK(unclassified.empty());

    // ---- 2. generate + bake -------------------------------------------------
    Sweep sw;
    for(const auto& f : files)
    {
      if(f.kind == Kind::Unclassified)
        continue;

      sw.per_kind[f.kind].files++;
      if(f.kind == Kind::VertexCompanion)
        continue; // baked as the vertex half of its fragment file's pair

#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
      // QShaderBaker::setMultiViewCount arrived in 6.7 and ShaderCache.cpp:92-95
      // guards its call on that version, so on an older Qt the baker has no
      // num_views to emit and every target rejects a MULTIVIEW shader --
      // including the Vulkan bake ProgramCache::get does on the way to the
      // ProcessedProgram, so it does not even survive GENERATION. Not a property
      // of the shader: skip it, by name, before generating.
      //
      // Detected from the ISF header rather than from descriptor.multiview_count
      // for exactly that reason -- the descriptor only exists after the
      // generation that would already have failed.
      if(declares_multiview(f))
      {
        sw.per_kind[f.kind].skipped++;
        sw.notes.push_back(
            "SKIP " + f.name.toStdString()
            + " (all targets): the shader declares MULTIVIEW, and "
              "QShaderBaker::setMultiViewCount -- which ShaderCache only calls "
              "on Qt >= 6.7 -- is what puts num_views into the baked shader. "
              "This build's Qt is older, so no target can bake it.");
        continue;
      }
#endif

      Generated g;
      switch(f.kind)
      {
        case Kind::Isf:
          g = generate_isf(f.path);
          break;
        case Kind::RawRaster:
          g = generate_raw_raster(f.companion, f.path);
          break;
        case Kind::Vsa:
          g = generate_vsa(f.path);
          break;
        case Kind::Csf:
          g = generate_csf(f.path);
          break;
        case Kind::GeometryFilter:
          g = generate_geometry_filter(f.path);
          break;
        default:
          continue;
      }
      sweep_one(sw, f, g);
    }

    // ---- 3. report ----------------------------------------------------------
    std::string report = "corpus bake sweep:\n";
    for(const auto& [kind, t] : sw.per_kind)
    {
      report += std::string{"  "} + kind_name(kind) + ": " + std::to_string(t.files)
                + " files, " + std::to_string(t.generated) + " generated, "
                + std::to_string(t.skipped) + " skipped, " + std::to_string(t.stages)
                + " stages, " + std::to_string(t.bakes_ok) + " bakes ok, "
                + std::to_string(t.gaps) + " known gaps\n";
    }
    report += "  " + std::to_string(sw.stages_total) + " generated stages\n";
    for(const auto& [lang, n] : sw.bakes_ok_per_lang)
      report += "  " + lang + ": " + std::to_string(n) + " stage bakes clean, "
                + std::to_string(sw.gaps_per_lang[lang]) + " known gaps\n";
    WARN(report << join(sw.notes));

    // ---- 4. verdicts --------------------------------------------------------
    INFO("shaders that do not bake:" << join(sw.failures));
    CHECK(sw.failures.empty());

    const Tally& isf = sw.per_kind[Kind::Isf];
    const Tally& raw = sw.per_kind[Kind::RawRaster];
    const Tally& vsa = sw.per_kind[Kind::Vsa];
    const Tally& csf = sw.per_kind[Kind::Csf];
    const Tally& geo = sw.per_kind[Kind::GeometryFilter];
    const Tally& comp = sw.per_kind[Kind::VertexCompanion];

    // ---- 5. the counts ------------------------------------------------------
    //
    // A green failure list proves nothing on its own: it is also what a sweep
    // over an empty set produces. Everything below exists so that a change
    // which stops generating a kind, stops classifying a kind, or stops
    // reaching HLSL and MSL at all, is VISIBLE as a number rather than as a
    // still-green run.

    // (a) What is on disk. Version-independent -- these are file counts, taken
    //     before anything is parsed. Raise them when the corpus grows; a drop
    //     is a regression to explain.
    CHECK(isf.files >= 48);
    CHECK(raw.files >= 24);
    CHECK(vsa.files == 2);
    CHECK(csf.files >= 30);
    CHECK(geo.files == 1);
    CHECK(comp.files >= 24);

    // (b) Every classified file was either generated or skipped with a stated
    //     reason -- no third, silent outcome.
    CHECK(isf.generated + isf.skipped == isf.files);
    CHECK(raw.generated + raw.skipped == raw.files);
    CHECK(vsa.generated + vsa.skipped == vsa.files);
    CHECK(csf.generated + csf.skipped == csf.files);
    CHECK(geo.generated + geo.skipped == geo.files);

    // (c) ...and every corpus file landed in exactly one bucket.
    CHECK(
        isf.files + raw.files + vsa.files + csf.files + geo.files + comp.files
        == int(files.size()) - int(unclassified.size()));

    // (d) The accounting identity that makes (a)-(c) bite: every generated
    //     stage was offered to every target, and each offer came back as
    //     exactly one of {clean, known gap, failure}. GLSL/SPIR-V/MSL are one
    //     target each; HLSL is two (Direct3D 11 and Direct3D 12).
    //
    //     This is the assertion that says HLSL and MSL -- the two languages no
    //     other test in a Linux ctest run ever asks glslang for, and the two
    //     the isf_FragCoord defect lived behind -- were really asked for, for
    //     every single generated stage.
    CHECK(sw.failures.empty());
    CHECK(sw.bakes_ok_per_lang["GLSL"] + sw.gaps_per_lang["GLSL"] == sw.stages_total);
    CHECK(
        sw.bakes_ok_per_lang["SPIR-V"] + sw.gaps_per_lang["SPIR-V"]
        == sw.stages_total);
    CHECK(sw.bakes_ok_per_lang["MSL"] + sw.gaps_per_lang["MSL"] == sw.stages_total);
    CHECK(
        sw.bakes_ok_per_lang["HLSL"] + sw.gaps_per_lang["HLSL"]
        == 2 * sw.stages_total);

    // (e) And an absolute floor under the stage count itself, so the identity
    //     in (d) cannot be satisfied by zero. 179 stages on a Qt that can bake
    //     multiview; 8 fewer (the four MULTIVIEW raw-raster pairs) below 6.7.
    CHECK(sw.stages_total >= 171);
  });
}
