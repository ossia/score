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

      if(!g_messages.isEmpty())
        failures["warning"] = g_messages.join(" | ").toStdString();
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

//! Runs one shader kind over the library and diffs against its baseline.
inline void sweepLibrary(
    const score::GUIApplicationContext& ctx, const QStringList& patterns,
    ProgramLoader load, const QString& baseline)
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

  g_previous = qInstallMessageHandler(capture);
  Sweeper sweeper{ctx.settings<Gfx::Settings::Model>().graphicsApiEnum()};

  std::map<QString, std::map<std::string, std::string>> failures;

  for(const QString& path : shaders)
  {
    const QString rel = QDir{root}.relativeFilePath(path);
    // Announce before rendering: on a backend that can hang or take the
    // process down, the last line printed names the shader responsible.
    qInfo().noquote() << "[sweep]" << rel;

    QFile f{path};
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    QString error;
    const auto program = load(path, f.readAll(), error);
    if(!program)
    {
      failures[rel][programErrorKind(error)]
          = error.isEmpty() ? "no program" : error.toStdString();
      report(rel, failures[rel]);
      continue;
    }

    if(auto res = sweeper.run(*program); !res.empty())
    {
      report(rel, res);
      failures[rel] = std::move(res);
    }
  }

  qInstallMessageHandler(g_previous);

  INFO("swept " << shaders.size() << " shaders, " << failures.size() << " failing");

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

  QStringList regressions;
  for(const auto& entry : current)
    if(!known.contains(entry))
      regressions.push_back(entry);

  INFO("new failures:\n" << regressions.join('\n').toStdString());
  CHECK(regressions.isEmpty());
}
}
