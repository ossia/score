#pragma once

// Shared machinery for the shader-library sweeps.
//
// Each shader kind in the library gets its own harness -- ISF fragment shaders,
// VertexShaderArt vertex shaders, CSF compute shaders -- because they are
// authored differently, fail differently, and want separate baselines. What
// they share is the pipeline: score::gfx::ISFNode dispatches on the parsed
// descriptor's mode to the matching renderer, so one Sweeper drives them all.
//
// See ShaderSweepISF.cpp for the full description of the categories reported
// and the environment variables understood.

#include <score_test/App.hpp>

#include <Library/LibrarySettings.hpp>

#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

#if defined(__EMSCRIPTEN__)
// score_lib_base is a static archive on wasm, so the resource initialiser that
// qt_add_resources generates is dropped unless something references it, and
// :/gfx/* then does not exist in the test binary.
inline void initScoreResources()
{
  Q_INIT_RESOURCE(score);
}
#else
inline void initScoreResources() { }
#endif

namespace
{
constexpr int frames_per_shader = 4;
constexpr QSize render_size{320, 240};

// What QRhi asks the baker for on WebGL2. The desktop profile comes from the
// RenderState the offscreen output actually created.
const QShaderVersion wasm_profile{300, QShaderVersion::GlslEs};

QStringList g_messages;
bool g_capturing{};
QtMessageHandler g_previous{};

// A shader is judged by its own diagnostics, not by everything else the frame
// logged. The KHR_debug stream, the plugin loader and the ISF/CSF fallback
// notices all arrive as qWarning and say nothing about the shader under test;
// only a message that names a failure does. A validation message that reports a
// real GL error still matches, so the guard keeps its teeth.
const QRegularExpression g_failure_re{
    QStringLiteral("error|fatal|failed|failure|cannot|unable|invalid"),
    QRegularExpression::CaseInsensitiveOption};

void capture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(g_capturing && type >= QtWarningMsg)
    g_messages.push_back(msg);
  if(g_previous)
    g_previous(type, ctx, msg);
}

struct MessageCapture
{
  MessageCapture()
  {
    g_messages.clear();
    g_capturing = true;
  }
  ~MessageCapture() { g_capturing = false; }
};

QString libraryRoot(const score::ApplicationContext& ctx)
{
  if(auto env = qEnvironmentVariable("SCORE_SHADER_LIBRARY_DIR"); !env.isEmpty())
    return env;
#if defined(SCORE_SHADER_SWEEP_WASM_LIBRARY)
  return QStringLiteral(SCORE_SHADER_SWEEP_WASM_LIBRARY);
#else
  return ctx.settings<Library::Settings::Model>().getDefaultLibraryPath();
#endif
}

QImage testcard()
{
  initScoreResources();
  QImage img{":/gfx/testcard-1.png"};
  // A flat fallback would make every passthrough filter look like it rendered
  // nothing, so say plainly which one the run used.
  qInfo().noquote() << "[sweep] testcard:"
                    << (img.isNull()
                            ? QStringLiteral("MISSING -> flat magenta fallback")
                            : QStringLiteral("loaded %1x%2")
                                  .arg(img.width())
                                  .arg(img.height()));
  if(img.isNull())
  {
    img = QImage{render_size, QImage::Format_RGBA8888};
    img.fill(Qt::magenta);
  }
  return img;
}

//! True when every pixel of the frame is the same colour.
bool isUniform(const QRhiReadbackResult& rb)
{
  const auto px = rb.pixelSize.width() * rb.pixelSize.height();
  if(px <= 1 || rb.data.size() < px * 4)
    return false;

  const auto* p = reinterpret_cast<const uint32_t*>(rb.data.constData());
  return std::all_of(p + 1, p + px, [first = p[0]](uint32_t v) { return v == first; });
}

//! Drives one shader through the whole pipeline, reusing the QRhi across calls.
struct Sweeper
{
  score::gfx::Graph graph;
  score::gfx::BackgroundNode output;
  score::gfx::GraphicsApi api;
  QImage image = testcard();

  explicit Sweeper(score::gfx::GraphicsApi a)
      : api{a}
  {
    output.shared_readback = std::make_shared<QRhiReadbackResult>();
    output.setRenderSize(render_size);
  }

  //! Failure kinds found for this shader, empty when it rendered.
  std::map<std::string, std::string> run(const Gfx::ProcessedProgram& program)
  {
    std::map<std::string, std::string> failures;

    // CSF carries its compute source in ProcessedProgram::fragment and has no
    // vertex/fragment pair: score::gfx::ISFNode has a separate constructor.
    auto isf = program.descriptor.mode == isf::descriptor::CSF
                   ? std::make_unique<score::gfx::ISFNode>(
                         program.descriptor, program.fragment)
                   : std::make_unique<score::gfx::ISFNode>(
                         program.descriptor, program.vertex, program.fragment);

    if(auto err = bakeForWasm(*isf); !err.isEmpty())
      failures["gles300"] = err.toStdString();

    std::vector<std::unique_ptr<score::gfx::Node>> images;

    {
      MessageCapture capture;
      try
      {
        graph.addNode(&output);
        graph.addNode(isf.get());
        graph.addEdge(
            isf->output[0], output.input[0], Process::CableType::ImmediateGlutton);

        for(auto* port : isf->input)
        {
          if(port->type != score::gfx::Types::Image)
            continue;
          auto node = new score::gfx::FullScreenImageNode{image};
          images.push_back(std::unique_ptr<score::gfx::Node>(node));
          graph.addNode(node);
          graph.addEdge(
              node->output[0], port, Process::CableType::ImmediateGlutton);
        }

        graph.createAllRenderLists(api);

        for(int i = 0; i < frames_per_shader; i++)
        {
          isf->standardUBO.frameIndex++;
          isf->standardUBO.time += 1. / 60.;
          isf->standardUBO.timeDelta = 1. / 60.;
          isf->standardUBO.progress += 1. / frames_per_shader;
          output.render();
        }
      }
      catch(const std::exception& e)
      {
        failures["render"] = e.what();
      }
      catch(...)
      {
        failures["render"] = "unknown exception";
      }

      if(const auto errs = g_messages.filter(g_failure_re); !errs.isEmpty())
        failures["warning"] = errs.join(" | ").toStdString();
    }

    if(auto st = output.renderState(); st && st->rhi && st->rhi->isDeviceLost())
      failures["devicelost"] = "QRhi reported device loss";

    if(!failures.count("render") && isUniform(*output.shared_readback))
      failures["blank"] = "every pixel identical";

    teardown(*isf, images);
    return failures;
  }

  //! The compute source is a template: RenderedCSFNode fills the work-group
  //! size in per pass, so the baker needs the first pass's values here.
  static QString withLocalSize(const score::gfx::ISFNode& isf)
  {
    QString src = isf.m_computeS;
    if(src.isEmpty() || isf.m_descriptor.csf_passes.empty())
      return src;

    const auto& ls = isf.m_descriptor.csf_passes[0].local_size;
    src.replace("ISF_LOCAL_SIZE_X", QString::number(ls[0]));
    src.replace("ISF_LOCAL_SIZE_Y", QString::number(ls[1]));
    src.replace("ISF_LOCAL_SIZE_Z", QString::number(ls[2]));
    return src;
  }

  //! Bakes the node's shaders for the WebAssembly GLSL profile.
  QString bakeForWasm(const score::gfx::ISFNode& isf) const
  {
    QString errors;
    const auto check
        = [&](const QString& src, QShader::Stage stage, const char* what) {
      if(src.isEmpty())
        return;
      const auto& [shader, error]
          = score::gfx::ShaderCache::get(api, wasm_profile, src.toUtf8(), stage);
      if(!error.isEmpty() || !shader.isValid())
      {
        if(!errors.isEmpty())
          errors += " | ";
        errors += QString{"%1: %2"}.arg(
            what, error.isEmpty() ? QStringLiteral("invalid shader") : error);
      }
    };

    check(isf.m_vertexS, QShader::VertexStage, "vertex");
    check(isf.m_fragmentS, QShader::FragmentStage, "fragment");
    check(withLocalSize(isf), QShader::ComputeStage, "compute");
    return errors;
  }

  void teardown(
      score::gfx::ISFNode& isf,
      const std::vector<std::unique_ptr<score::gfx::Node>>& images)
  {
    graph.clearEdges();
    for(auto& node : images)
      graph.removeNode(node.get());
    graph.removeNode(&isf);
    graph.removeNode(&output);

    // Tears the render lists down while the nodes are still alive.
    graph.createAllRenderLists(api);
  }
};


//! Writes the last frame a shader produced, so that "it rendered" can be
//! checked against what the shader is supposed to draw rather than taken on
//! faith. Off unless SCORE_SHADER_SWEEP_DUMP_DIR names a directory.
void dumpFrame(
    const QString& shader, const std::shared_ptr<QRhiReadbackResult>& rb_p)
{
  static const QString dir = qEnvironmentVariable("SCORE_SHADER_SWEEP_DUMP_DIR");
  if(dir.isEmpty() || !rb_p)
    return;

  const auto& rb = *rb_p;
  const auto px = rb.pixelSize.width() * rb.pixelSize.height();
  if(px <= 0 || rb.data.size() != px * 4)
    return;

  QString name = shader;
  name.replace('/', '_');

  QDir{}.mkpath(dir);
  const QImage img{
      reinterpret_cast<const uchar*>(rb.data.constData()), rb.pixelSize.width(),
      rb.pixelSize.height(), QImage::Format_RGBA8888};
  img.copy().save(dir + '/' + name + ".png");
}

//! The MODE declared in a shader's JSON header, or an empty string when it has
//! none (plain ISF). Which sweep owns a file is decided by this, not by its
//! extension: RAW_RASTER_PIPELINE, COMPUTE_SHADER and VERTEX_SHADER_ART shaders
//! are all written as .fs/.vs, and routing them into the ISF loader compiles
//! them against the wrong prelude -- 41 of the testers failed that way, while
//! the subsystem they were written for went entirely unexercised.
inline QString shaderMode(const QByteArray& data)
{
  static const QRegularExpression re{
      R"_("MODE"\s*:\s*"([A-Z_]+)")_"};
  const auto m = re.match(QString::fromUtf8(data.left(8192)));
  return m.hasMatch() ? m.captured(1) : QString{};
}

//! ProgramCache reports both ISF parsing and shader baking through one string.
const char* programErrorKind(const QString& error)
{
  return error.startsWith("Vertex shader error")
                 || error.startsWith("Fragment shader error")
             ? "bake"
             : "parse";
}

//! WebGL2 is OpenGL ES 3.0; ask for that profile rather than desktop GL.
void requestGlesContext()
{
  if(!qEnvironmentVariableIsSet("SCORE_SHADER_SWEEP_GLES"))
    return;

  if(!qEnvironmentVariableIsSet("QT_XCB_GL_INTEGRATION"))
    qputenv("QT_XCB_GL_INTEGRATION", "xcb_egl");

  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setRenderableType(QSurfaceFormat::OpenGLES);
  fmt.setVersion(3, 0);
  QSurfaceFormat::setDefaultFormat(fmt);
}
}


namespace
{
//! Loads one shader file into a program, or reports why it could not be.
using ProgramLoader
    = std::optional<Gfx::ProcessedProgram> (*)(const QString& path, QByteArray data,
                                               QString& error);

//! The baseline records only the failure kinds; the message that produced each
//! one is only ever seen live, so print it as the run goes.
inline void
report(const QString& shader, const std::map<std::string, std::string>& kinds)
{
  for(const auto& [kind, message] : kinds)
    qInfo().noquote() << "[sweep!]" << shader << QString::fromStdString(kind)
                      << QString::fromStdString(message);
}

//! The baseline records only the failure KINDS per file, so the test fails on
//! *new* failures rather than on a known-bad corpus. Shared by every sweep.
inline void diffAgainstBaseline(
    const std::map<QString, std::map<std::string, std::string>>& failures,
    const QString& baseline)
{
  QStringList current;
  for(const auto& [file, kinds] : failures)
    for(const auto& [kind, _] : kinds)
      current.push_back(file + '\t' + QString::fromStdString(kind));
  current.sort();

  if(qEnvironmentVariableIsSet("SCORE_SHADER_SWEEP_WRITE_BASELINE"))
  {
    QFile out{baseline};
    REQUIRE(out.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream{&out} << current.join('\n') << '\n';
    return;
  }

  QStringList known;
  if(QFile in{baseline}; in.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    known = QString::fromUtf8(in.readAll()).split('\n', Qt::SkipEmptyParts);
    known.sort();
  }
  else
  {
    FAIL(
        "no baseline at " << baseline.toStdString()
                          << ": run with SCORE_SHADER_SWEEP_WRITE_BASELINE to create it");
  }

  QStringList regressions;
  for(const auto& entry : current)
    if(!known.contains(entry))
      regressions.push_back(entry);

  INFO("new failures:\n" << regressions.join('\n').toStdString());
  CHECK(regressions.isEmpty());
}

//! Runs one shader kind over the library and diffs against its baseline.
//! @p wantMode selects which files this sweep owns: an empty string means "no
//! MODE header at all", i.e. plain ISF. Files declaring another mode are skipped,
//! not failed.
//! @p blankIsFailure says whether "every pixel identical" means anything for this
//! kind of shader. It does for ISF, which draws a full-screen pass on its own. It
//! does NOT for a raster pipeline: those draw geometry, and this harness wires no
//! geometry producer, so they legitimately render nothing here. Counting that as a
//! failure would measure the harness, not the shader — pixel validation for raster
//! belongs to the JS-wiring harness, which assembles the whole scene chain.
inline void sweepLibrary(
    const score::GUIApplicationContext& ctx, const QStringList& patterns,
    ProgramLoader load, const QString& baseline, const QString& wantMode = {},
    bool blankIsFailure = true)
{
  const QString root = libraryRoot(ctx);
  if(root.isEmpty() || !QFileInfo::exists(root))
    SKIP("no shader library available (set SCORE_SHADER_LIBRARY_DIR)");

  QStringList shaders;
  QDirIterator it{
      root, patterns, QDir::Files,
      QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
  while(it.hasNext())
    shaders.push_back(it.next());
  shaders.sort();

  if(shaders.isEmpty())
    SKIP("no shaders of this kind in the library");

  // The backend comes from the gfx settings model, not from the environment:
  // Gfx::Settings::Model reads QSG_RHI_BACKEND at construction and unsets it
  // straight away, so by the time we get here the environment no longer says
  // anything. score::gfx::BackgroundNode reads the same model in its
  // constructor, so there is no rendering to be had without it either.
  const auto* gfx_settings = ctx.findSettings<Gfx::Settings::Model>();
  if(!gfx_settings)
    FAIL(
        "score_plugin_gfx registered no settings model: the gfx plug-in was not "
        "loaded. Plug-ins are discovered in <cwd>/plugins -- run this from the "
        "build root, as ctest does.");

  g_previous = qInstallMessageHandler(capture);
  Sweeper sweeper{gfx_settings->graphicsApiEnum()};

  std::map<QString, std::map<std::string, std::string>> failures;

  for(const QString& path : shaders)
  {
    const QString rel = QDir{root}.relativeFilePath(path);

    QFile f{path};
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;
    const QByteArray data = f.readAll();

    // Skip, do not fail, a shader another sweep owns. Compiling a
    // RAW_RASTER_PIPELINE against the ISF prelude only ever produces
    // "'position' : undeclared identifier", which says nothing about the shader.
    if(shaderMode(data) != wantMode)
      continue;

    // Announce before rendering: on a backend that can hang or take the
    // process down, the last line printed names the shader responsible.
    qInfo().noquote() << "[sweep]" << rel;

    QString error;
    const auto program = load(path, data, error);
    if(!program)
    {
      failures[rel][programErrorKind(error)]
          = error.isEmpty() ? "no program" : error.toStdString();
      report(rel, failures[rel]);
      continue;
    }

    auto res = sweeper.run(*program);
    if(!blankIsFailure)
      res.erase("blank");
    dumpFrame(rel, sweeper.output.shared_readback);
    if(!res.empty())
    {
      report(rel, res);
      failures[rel] = std::move(res);
    }
  }

  qInstallMessageHandler(g_previous);

  INFO("swept " << shaders.size() << " shaders, " << failures.size() << " failing");

  diffAgainstBaseline(failures, baseline);
}
}
