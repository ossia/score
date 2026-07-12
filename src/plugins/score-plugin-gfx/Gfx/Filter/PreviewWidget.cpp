#include <Process/Preset.hpp>

#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Widgets/RhiPreviewWidget.hpp>

#include <score/application/ApplicationContext.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/value/value.hpp>

#include <QApplication>
#include <QTimer>

#include <rnd/random.hpp>

namespace Gfx
{
namespace
{
// Basic "synth" to generate a fake waveform for audio reactive shader previews
class FakeMusicGenerator
{
private:
  double bassPhase = 0.0;
  float kickEnv = 0.0f;
  float snareEnv = 0.0f;
  float bassEnv = 0.0f;
  int sampleCounter = 0;

  float last{};

public:
  void operator()(float* buffer, size_t numSamples)
  {
    const float FS = 48000.;
    const double twoPi = 2.0 * M_PI;
    const double bassFreq = rnd::rand(1000., 5000.);
    const double bassInc = twoPi * bassFreq / FS;
    const int samplesPerBeat = FS / 2;

    for(size_t i = 0; i < numSamples; i++)
    {
      if(sampleCounter % samplesPerBeat == 0)
        kickEnv = 1.0f;
      if(sampleCounter % samplesPerBeat == samplesPerBeat / 2)
        snareEnv = 0.8f;
      if(sampleCounter % samplesPerBeat == samplesPerBeat / 3)
        bassEnv = 0.5f;

      float bass = 0.f;
      if(bassEnv > 0.01f)
      {
        bass += (std::sin(bassPhase)
                 + std::sin(bassPhase) * std::sin(std::sin(bassPhase)))
                * std::pow(bassEnv, 2.f) * 0.1f;
        bassEnv *= 0.9995f;
      }

      float kick = 0.0f;
      if(kickEnv > 0.01f)
      {
        float kickPitch = 100.0 + kickEnv * 30.0;
        kick = std::sin(twoPi * kickPitch * sampleCounter / FS) * kickEnv * 0.5f;
        kickEnv *= 0.9f;
      }

      float snare = 0.0f;
      if(snareEnv > 0.01f)
      {
        snare = rnd::rand(-1., 1.) * snareEnv * 0.3f;
        snareEnv *= 0.9f;
      }

      float hihat = 0.0;
      if((sampleCounter % (samplesPerBeat / 4)) < 10)
      {
        hihat = rnd::rand(-1., 1.);
      }

      float sample = kick + snare + bass + hihat;
      sample = (0.005 * sample) + (1. - 0.005) * this->last;
      last = sample;

      sample = std::tanh(sample * 0.5f);

      buffer[i] = sample;

      bassPhase += bassInc;

      if(bassPhase > twoPi)
        bassPhase -= twoPi;
      sampleCounter++;
    }
  }
};

struct PreviewInputVisitor
{
  int& img_count;

  score::gfx::NodeModel* operator()(const isf::float_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::long_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::event_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::bool_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::point2d_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::point3d_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::color_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::image_input& v)
  {
    static std::array<QImage, 3> images{
        QImage{":/gfx/testcard-1.png"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
        QImage{":/gfx/testcard-2.jpeg"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
        QImage{":/gfx/testcard-3.png"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)};
    auto image_node = new score::gfx::FullScreenImageNode{images[img_count]};
    img_count++;
    img_count = img_count % 3;
    return image_node;
  }
  score::gfx::NodeModel* operator()(const isf::cubemap_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::audio_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::audioFFT_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::audioHist_input& v) { return nullptr; }
  
  // CSF-specific input handlers
  score::gfx::NodeModel* operator()(const isf::storage_input& v) { return nullptr; }
  score::gfx::NodeModel* operator()(const isf::uniform_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::texture_input& v)
  {
    static std::array<QImage, 3> images{
        QImage{":/gfx/testcard-1.png"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
        QImage{":/gfx/testcard-2.jpeg"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
        QImage{":/gfx/testcard-3.png"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)};
    auto image_node = new score::gfx::FullScreenImageNode{images[img_count]};
    img_count++;
    img_count = img_count % 3;
    return image_node;
  }
  
  score::gfx::NodeModel* operator()(const isf::csf_image_input& v)
  {
    static std::array<QImage, 3> images{
        QImage{":/gfx/testcard-1.png"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
        QImage{":/gfx/testcard-2.jpeg"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
        QImage{":/gfx/testcard-3.png"}.scaled(
            300, 200, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)};
    auto image_node = new score::gfx::FullScreenImageNode{images[img_count]};
    img_count++;
    img_count = img_count % 3;
    return image_node;
  }

  score::gfx::NodeModel* operator()(const isf::geometry_input& v) { return nullptr; }
};

struct PreviewPresetVisitor
{
  score::gfx::ISFNode& node;
  ossia::flat_map<int, ossia::value>& controls;
  // Descriptor-input index: matches both the saved preset control keys
  // (model inlet id == desc.inputs index, see setupISFModelPorts) and the
  // controls flat_map key.
  int i{};
  // Render-port index: index into node.input[], advanced via
  // walk_descriptor_inputs (an input may create 0 or 2 ports, so this
  // drifts from the descriptor index).
  int port{};

  // Guarded material pointer for the current render port: nullptr if the
  // port index is out of range or the port carries no material storage.
  float* portValue() const noexcept
  {
    if(port < 0 || port >= (int)node.input.size())
      return nullptr;
    return reinterpret_cast<float*>(node.input[port]->value);
    // NB: for scalar/vector inputs value always points into the material
    // UBO blob; image/audio inputs never reach this (their visitors no-op).
  }

  void operator()(const isf::float_input& v)
  {
    if(float* dst = portValue(); dst)
      if(float* val = controls[i].target<float>())
        *dst = *val;
  }

  void operator()(const isf::long_input& v)
  {
    if(float* dst = portValue(); dst)
      if(int* val = controls[i].target<int>())
        *reinterpret_cast<int*>(dst) = *val;
  }

  void operator()(const isf::event_input& v) { }

  void operator()(const isf::bool_input& v)
  {
    if(float* dst = portValue(); dst)
      if(bool* val = controls[i].target<bool>())
        *reinterpret_cast<int*>(dst) = *val ? 1 : 0;
  }

  void operator()(const isf::point2d_input& v)
  {
    if(float* dst = portValue(); dst)
      if(ossia::vec2f* val = controls[i].target<ossia::vec2f>())
      {
        dst[0] = (*val)[0];
        dst[1] = (*val)[1];
      }
  }

  void operator()(const isf::point3d_input& v)
  {
    if(float* dst = portValue(); dst)
      if(ossia::vec3f* val = controls[i].target<ossia::vec3f>())
      {
        dst[0] = (*val)[0];
        dst[1] = (*val)[1];
        dst[2] = (*val)[2];
      }
  }

  void operator()(const isf::color_input& v)
  {
    if(float* dst = portValue(); dst)
      if(ossia::vec4f* val = controls[i].target<ossia::vec4f>())
      {
        dst[0] = (*val)[0];
        dst[1] = (*val)[1];
        dst[2] = (*val)[2];
        dst[3] = (*val)[3];
      }
  }

  void operator()(const isf::image_input& v) { }

  void operator()(const isf::cubemap_input& v) { }

  void operator()(const isf::audio_input& v) { }

  void operator()(const isf::audioFFT_input& v) { }

  void operator()(const isf::audioHist_input& v) { }
  
  // CSF-specific input handlers
  void operator()(const isf::storage_input& v) { }
  void operator()(const isf::uniform_input& v) { }

  void operator()(const isf::texture_input& v) { }

  void operator()(const isf::csf_image_input& v) { }

  void operator()(const isf::geometry_input& v) { }
};
}

ShaderPreviewManager* g_shaderPreview{};
bool g_shaderPreviewScheduledForDeletion{};

// Holds the source ISF + image nodes shared across hover previews.
// The output side is owned by individual ShaderPreviewWidget /
// RhiPreviewWidget instances: each contributes a score::gfx::PreviewNode
// targeting its own QRhiWidget render target. Multiple previews can be
// attached at once (e.g. library hover + live texture-port preview).
class ShaderPreviewManager : public QObject
{
public:
  ShaderPreviewManager()
      : QObject{qApp}
  {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [] {
      delete g_shaderPreview;
      g_shaderPreviewScheduledForDeletion = false;
    });
  }

  ~ShaderPreviewManager()
  {
    g_shaderPreview = nullptr;
    g_shaderPreviewScheduledForDeletion = false;
  }

  void load(const QString& path)
  {
    ShaderSource program;
    if(path.contains(".fs") || path.contains(".frag"))
      program = programFromISFFragmentShaderPath(path, {});
    if(path.contains(".vs") || path.contains(".vert"))
      program = programFromVSAVertexShaderPath(path, {});

    if(const auto& [processed, error]
       = ProgramCache::instance().get(program, path);
       bool(processed))
    {
      m_program = *processed;
      setup();
    }
  }

  void load(const Process::Preset& preset)
  {
    const rapidjson::Document doc = readJson(preset.data);
    if(!doc.IsObject())
      return;
    auto obj = doc.GetObject();
    if(!obj.HasMember("Fragment") || !obj.HasMember("Vertex"))
      return;
    ShaderSource::ProgramType type = ShaderSource::ProgramType::ISF;
    if(obj.HasMember("Type"))
      type = (ShaderSource::ProgramType)obj["Type"].GetInt();
    auto frag = obj["Fragment"].GetString();
    auto vert = obj["Vertex"].GetString();
    ShaderSource program{type, vert, frag};

    // Preset-loaded source has no origin file; includes resolve against
    // global search paths only.
    if(const auto& [processed, error] = ProgramCache::instance().get(program);
       bool(processed))
    {
      m_program = *std::move(processed);
      setup();
      if(m_isf)
      {
        ossia::flat_map<int, ossia::value> controls;
        for(const auto& arr : obj["Controls"].GetArray())
        {
          controls[arr[0].GetInt()] = JsonValue{arr[1]}.to<ossia::value>();
        }

        // controls is keyed by descriptor-input index (== model inlet id);
        // node.input[] is keyed by render-port index. walk_descriptor_inputs
        // gives the render-port index (cur.inlets) for each descriptor entry,
        // which drifts from the descriptor index for 0-/2-port inputs.
        int i = 0;
        score::gfx::walk_descriptor_inputs(
            m_program.descriptor,
            [&](const isf::input& input, const score::gfx::port_counts& cur,
                const score::gfx::port_counts&) {
              ossia::visit(
                  PreviewPresetVisitor{*m_isf, controls, i, cur.inlets},
                  input.data);
              i++;
            });
      }
    }
  }

  score::gfx::Graph& graph() noexcept { return m_graph; }

  // True while at least one preview widget is still attached to the shared
  // graph. The deferred manager deletion must NOT fire while this holds, or
  // a surviving widget's RhiPreviewWidget::m_graph would dangle (UAF on its
  // detach()).
  bool hasPreviews() const noexcept { return !m_previews.empty(); }

  void attachPreview(score::gfx::BackgroundNode& node)
  {
    m_previews.push_back(&node);
    if(m_isf)
    {
      m_graph.addEdge(
          m_isf->output[0], node.input[0], Process::CableType::ImmediateGlutton);
      const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
      m_graph.createAllRenderLists(settings.graphicsApiEnum());
    }
  }

  void detachPreview(score::gfx::BackgroundNode& node)
  {
    ossia::remove_erase(m_previews, &node);
    if(m_isf)
      m_graph.removeEdge(m_isf->output[0], node.input[0]);
  }

  std::vector<std::pair<score::gfx::Port*, score::gfx::Port*>> m_previewEdges;

  void setup()
  {
    const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
    // Tear down the previous set of source nodes.
    for(auto [a, b] : m_previewEdges)
      m_graph.removeEdge(a, b);
    m_previewEdges.clear();

    // FIXME memleak ?
    for(auto& in : m_textures)
    {
      m_graph.removeNode(in.get());
    }

    if(m_isf)
    {
      for(auto* p : m_previews)
        m_graph.removeEdge(m_isf->output[0], p->input[0]);
      m_graph.removeNode(m_isf.get());
    }

    // Clear the graph, renderers etc.
    m_graph.createAllRenderLists(settings.graphicsApiEnum());

    m_isf.reset();
    m_textures.clear();

    // FIXME add an error image if the shader did not parse
    m_isf = std::make_unique<score::gfx::ISFNode>(
        m_program.descriptor, m_program.vertex, m_program.fragment);

    m_graph.addNode(m_isf.get());

    // Wire ISF output to every currently-attached preview.
    for(auto* p : m_previews)
      m_graph.addEdge(
          m_isf->output[0], p->input[0], Process::CableType::ImmediateGlutton);

    // Edges from image nodes to image inputs. The render-port index of an
    // input (cur.inlets, via walk_descriptor_inputs) drifts from the
    // descriptor index for inputs that create 0 or 2 ports, so we must not
    // equate them. PreviewInputVisitor only yields a node for image-like
    // inputs, each of which creates exactly one input port at cur.inlets.
    int image_i = 0;
    score::gfx::walk_descriptor_inputs(
        m_program.descriptor,
        [&](const isf::input& input, const score::gfx::port_counts& cur,
            const score::gfx::port_counts& delta) {
          auto node = ossia::visit(PreviewInputVisitor{image_i}, input.data);
          if(node)
          {
            const int port_idx = cur.inlets;
            // Only wire when this input actually creates an input port:
            // write-access csf_image_input yields a node but 0 inlets, and
            // the render-port index must come from cur.inlets (not the
            // descriptor index, which drifts for 0-/2-port inputs).
            if(delta.inlets < 1 || port_idx < 0
               || port_idx >= (int)m_isf->input.size())
            {
              delete node;
              return;
            }

            m_graph.addNode(node);

            m_graph.addEdge(
                node->output[0], m_isf->input[port_idx],
                Process::CableType::ImmediateGlutton);
            m_previewEdges.emplace_back(node->output[0], m_isf->input[port_idx]);

            m_textures.push_back(std::unique_ptr<score::gfx::Node>(node));
          }
        });

    m_graph.createAllRenderLists(settings.graphicsApiEnum());
  }

  void updateControls()
  {
    if(!m_isf)
      return;

    m_isf->standardUBO.frameIndex++;
    m_isf->standardUBO.time += 16. / 1000.;
    m_isf->standardUBO.timeDelta = 16. / 1000.;
    m_isf->standardUBO.progress += 0.002;

    // OPTIMIZEME
    {
      // ISF nodes with two images inputs (transitions)
      // generally have a "progress" uniform to indicate the
      // progress of the transition ; try to leverage it to show the transition.
      std::size_t progress_k = 0;
      bool has_progress = false;
      for(auto& input : m_isf->descriptor().inputs)
      {
        if(input.name == "progress")
        {
          has_progress = true;
          break;
        }
        progress_k++;
      }

      for(auto in : m_isf->input)
      {
        if(in->type == score::gfx::Types::Audio)
        {
          auto tex = (score::gfx::AudioTexture*)in->value;
          tex->data.resize(512);
          tex->channels = 1;

          static FakeMusicGenerator generator;
          generator(tex->data.data(), 512);
        }
      }

      if(has_progress)
      {
        if(m_isf->input.size() > progress_k)
        {
          if(m_isf->input[progress_k]->type == score::gfx::Types::Float)
          {
            static float progress = 0;
            progress += 0.01;
            if(progress >= 1.0)
              progress = 0.;
            *((float*)m_isf->input[progress_k]->value) = progress;
          }
        }
        m_isf->materialChange();
      }
    }
  }

private:
  std::unique_ptr<score::gfx::ISFNode> m_isf{};
  std::vector<std::unique_ptr<score::gfx::Node>> m_textures;
  std::vector<score::gfx::BackgroundNode*> m_previews;
  score::gfx::Graph m_graph{};
  ProcessedProgram m_program;
};

ShaderPreviewWidget::ShaderPreviewWidget(const QString& path, QWidget* parent)
    : QWidget{parent}
{
  g_shaderPreviewScheduledForDeletion = false;
  if(!g_shaderPreview)
  {
    g_shaderPreview = new ShaderPreviewManager;
  }
  g_shaderPreview->load(path);
  setup();
}

ShaderPreviewWidget::ShaderPreviewWidget(const Process::Preset& preset, QWidget* parent)
    : QWidget{parent}
{
  g_shaderPreviewScheduledForDeletion = false;
  if(!g_shaderPreview)
  {
    g_shaderPreview = new ShaderPreviewManager;
  }
  g_shaderPreview->load(preset);
  setup();
}

ShaderPreviewWidget::~ShaderPreviewWidget()
{
  // Tearing down the RhiPreviewWidget triggers detachPreview() on the
  // manager, which removes the producer→preview edge. Do this before
  // scheduling manager deletion so the deferred delete sees a clean
  // graph.
  delete m_rhi;
  m_rhi = nullptr;

  g_shaderPreviewScheduledForDeletion = true;
  QTimer::singleShot(std::chrono::seconds(5), qApp, []() {
    // Multi-client safety: several ShaderPreviewWidgets can share the same
    // manager (library hover + live texture-port preview). Destroying one
    // schedules this deletion, but another may still be attached — its
    // RhiPreviewWidget holds a raw pointer into g_shaderPreview->graph().
    // Only tear the manager down once no preview remains attached, otherwise
    // the surviving widget would dereference a freed Graph on its own
    // destruction (use-after-free).
    if(g_shaderPreviewScheduledForDeletion && g_shaderPreview
       && !g_shaderPreview->hasPreviews())
    {
      delete g_shaderPreview;
      g_shaderPreview = nullptr;
      g_shaderPreviewScheduledForDeletion = false;
    }
  });
}

void ShaderPreviewWidget::setup()
{
  // UI setup
  auto lay = new QHBoxLayout(this);
  m_rhi = new RhiPreviewWidget(this);
  m_rhi->setMinimumSize(300, 200);
  m_rhi->setMaximumSize(300, 200);
  m_rhi->useGraph(
      &g_shaderPreview->graph(),
      [](score::gfx::BackgroundNode& n) {
        if(g_shaderPreview)
          g_shaderPreview->attachPreview(n);
      },
      [](score::gfx::BackgroundNode& n) {
        if(g_shaderPreview)
          g_shaderPreview->detachPreview(n);
      });
  lay->addWidget(m_rhi);

  // Drives ISF time/progress uniforms. Frame submission is owned by
  // the QRhiWidget (it calls update() each render).
  startTimer(16);
}

void ShaderPreviewWidget::timerEvent(QTimerEvent* event)
{
  if(g_shaderPreview)
    g_shaderPreview->updateControls();
}
}
