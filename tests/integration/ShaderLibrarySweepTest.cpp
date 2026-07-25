// Renders every ISF shader in the user library through the real graphics
// pipeline: ProgramCache -> ISFNode -> Graph -> RenderList -> offscreen output,
// exactly as a Gfx filter process does at runtime, then reads the frame back.
//
// This deliberately goes further than parsing. A shader can translate cleanly
// and still fail when a pipeline is built for it, when a texture is uploaded in
// a format the backend does not accept, or by drawing nothing at all — none of
// which is visible before a frame is drawn.
//
// Each shader is additionally baked for GLSL ES 3.00, the profile the
// WebAssembly build gets, so a shader that only fails there shows up as a
// wasm-only breakage. SCORE_SHADER_SWEEP_GLES=1 goes further and runs the whole
// sweep on an OpenGL ES context instead of desktop GL.
//
// Failures are reported per shader as one of: parse (not valid ISF), bake (the
// shader does not compile), gles300 (compiles for desktop but not for the wasm
// profile), render (the pipeline threw), warning (the backend complained),
// devicelost, blank (the frame came back one flat colour).
//
// The library is not part of the repository, so the test skips when it is
// absent. Point it somewhere explicitly with SCORE_SHADER_LIBRARY_DIR.
// Known-bad shaders are tolerated through a baseline file: the test fails on
// *new* failures only. Refresh it with SCORE_SHADER_SWEEP_WRITE_BASELINE=1.
// SCORE_SHADER_SWEEP_LOG=<path> traces each shader before it is rendered, so a
// shader that takes the process down with it can still be identified.

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
  return ctx.settings<Library::Settings::Model>().getDefaultLibraryPath();
}

QImage testcard()
{
  QImage img{":/gfx/testcard-1.png"};
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

    auto isf = std::make_unique<score::gfx::ISFNode>(
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
    check(isf.m_computeS, QShader::ComputeStage, "compute");
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

QString baselinePath()
{
  return QStringLiteral(SCORE_SHADER_SWEEP_BASELINE);
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

TEST_CASE("Every ISF shader in the library renders", "[integration][gfx][shaders]")
{
  requestGlesContext();

  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const QString root = libraryRoot(ctx);
    if(root.isEmpty() || !QFileInfo::exists(root))
      SKIP("no shader library available (set SCORE_SHADER_LIBRARY_DIR)");

    QStringList shaders;
    QDirIterator it{
        root, {"*.fs", "*.frag"}, QDir::Files,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
    while(it.hasNext())
      shaders.push_back(it.next());
    shaders.sort();

    REQUIRE(!shaders.isEmpty());

    QFile trace{qEnvironmentVariable("SCORE_SHADER_SWEEP_LOG")};
    const bool tracing = !trace.fileName().isEmpty()
                         && trace.open(QIODevice::WriteOnly | QIODevice::Text);

    g_previous = qInstallMessageHandler(capture);

    Sweeper sweeper{
        ctx.settings<Gfx::Settings::Model>().graphicsApiEnum()};

    // file -> failure kind -> message
    std::map<QString, std::map<std::string, std::string>> failures;

    for(const QString& path : shaders)
    {
      const QString rel = QDir{root}.relativeFilePath(path);
      if(tracing)
      {
        QTextStream{&trace} << rel << '\n';
        trace.flush();
      }

      QFile f{path};
      if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
        continue;

      const auto source
          = Gfx::programFromISFFragmentShaderPath(path, f.readAll());
      const auto& [program, error] = Gfx::ProgramCache::instance().get(source);
      if(!program)
      {
        failures[rel][programErrorKind(error)]
            = error.isEmpty() ? "no program" : error.toStdString();
        continue;
      }

      if(auto res = sweeper.run(*program); !res.empty())
        failures[rel] = std::move(res);
    }

    qInstallMessageHandler(g_previous);

    std::set<QString> wasm_only;
    for(const auto& [file, kinds] : failures)
      if(kinds.size() == 1 && kinds.count("gles300"))
        wasm_only.insert(file);

    INFO(
        "swept " << shaders.size() << " shaders, " << failures.size()
                 << " failing, " << wasm_only.size() << " only on gles300");

    QStringList current;
    for(const auto& [file, kinds] : failures)
      for(const auto& [kind, _] : kinds)
        current.push_back(file + '\t' + QString::fromStdString(kind));
    current.sort();

    if(qEnvironmentVariableIsSet("SCORE_SHADER_SWEEP_WRITE_BASELINE"))
    {
      QFile out{baselinePath()};
      REQUIRE(out.open(QIODevice::WriteOnly | QIODevice::Text));
      QTextStream{&out} << current.join('\n') << '\n';
      return;
    }

    QStringList baseline;
    if(QFile in{baselinePath()}; in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      baseline = QString::fromUtf8(in.readAll()).split('\n', Qt::SkipEmptyParts);
      baseline.sort();
    }

    QStringList regressions;
    for(const auto& entry : current)
      if(!baseline.contains(entry))
        regressions.push_back(entry);

    INFO("new failures:\n" << regressions.join('\n').toStdString());
    CHECK(regressions.isEmpty());
  });
}
