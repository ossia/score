#include <Process/Preset.hpp>

#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>

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

  score::gfx::NodeModel* operator()(const isf::audio_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::audioFFT_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::audioHist_input& v) { return nullptr; }
  
  // CSF-specific input handlers
  score::gfx::NodeModel* operator()(const isf::storage_input& v) { return nullptr; }
  
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
};

struct PreviewPresetVisitor
{
  score::gfx::ISFNode& node;
  ossia::flat_map<int, ossia::value>& controls;
  int i{};
  void operator()(const isf::float_input& v)
  {
    if(float* v = controls[i].target<float>())
    {
      (*(float*)node.input[i]->value) = *v;
    }
  }

  void operator()(const isf::long_input& v)
  {
    if(int* v = controls[i].target<int>())
    {
      (*(int*)node.input[i]->value) = *v;
    }
  }

  void operator()(const isf::event_input& v) { }

  void operator()(const isf::bool_input& v)
  {
    if(bool* v = controls[i].target<bool>())
    {
      (*(int*)node.input[i]->value) = *v ? 1 : 0;
    }
  }

  void operator()(const isf::point2d_input& v)
  {
    if(ossia::vec2f* v = controls[i].target<ossia::vec2f>())
    {
      (*(float*)node.input[i]->value) = (*v)[0];
      (*((float*)node.input[i]->value + 1)) = (*v)[1];
    }
  }

  void operator()(const isf::point3d_input& v)
  {
    if(ossia::vec3f* v = controls[i].target<ossia::vec3f>())
    {
      (*(float*)node.input[i]->value) = (*v)[0];
      (*((float*)node.input[i]->value + 1)) = (*v)[1];
      (*((float*)node.input[i]->value + 2)) = (*v)[2];
    }
  }

  void operator()(const isf::color_input& v)
  {
    if(ossia::vec4f* v = controls[i].target<ossia::vec4f>())
    {
      (*(float*)node.input[i]->value) = (*v)[0];
      (*((float*)node.input[i]->value + 1)) = (*v)[1];
      (*((float*)node.input[i]->value + 2)) = (*v)[2];
      (*((float*)node.input[i]->value + 3)) = (*v)[3];
    }
  }

  void operator()(const isf::image_input& v) { }

  void operator()(const isf::audio_input& v) { }

  void operator()(const isf::audioFFT_input& v) { }

  void operator()(const isf::audioHist_input& v) { }
  
  // CSF-specific input handlers
  void operator()(const isf::storage_input& v) { }
  
  void operator()(const isf::texture_input& v) { }
  
  void operator()(const isf::csf_image_input& v) { }
};
}

ShaderPreviewManager* g_shaderPreview{};
bool g_shaderPreviewScheduledForDeletion{};

// Creating and destroying QRhi is fairly expensive, so
// we keep one around when we are showing ISF previews
class ShaderPreviewManager : public QObject
{
public:
  ShaderPreviewManager()
      : QObject{qApp}
  {
    score::gfx::OutputNode::Configuration conf{};
    m_screen = std::make_unique<score::gfx::ScreenNode>(conf, true);
    m_graph.addNode(m_screen.get());
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

    if(const auto& [processed, error] = ProgramCache::instance().get(program);
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

        int i = 0;
        for(const isf::input& input : m_program.descriptor.inputs)
        {
          ossia::visit(PreviewPresetVisitor{*m_isf, controls, i}, input.data);
          i++;
        }
      }
    }
  }

  std::shared_ptr<QWindow> getWindow()
  {
    if(m_screen && m_screen.get())
      return m_screen.get()->window();
    return {};
  }

  std::vector<std::pair<score::gfx::Port*, score::gfx::Port*>> m_previewEdges;

  void setup()
  {
    const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
    // Create our graph
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
      m_graph.removeEdge(m_isf->output[0], m_screen->input[0]);
      m_graph.removeNode(m_isf.get());
    }

    m_graph.removeNode(m_screen.get());

    // Clear the graph, renderers etc.
    m_graph.createAllRenderLists(settings.graphicsApiEnum());

    m_isf.reset();
    m_textures.clear();

    // Recreate what we need
    m_graph.addNode(m_screen.get());

    // FIXME add an error image if the shader did not parse
    m_isf = std::make_unique<score::gfx::ISFNode>(
        m_program.descriptor, m_program.vertex, m_program.fragment);

    m_graph.addNode(m_isf.get());
    // Edge from filter to output
    m_graph.addEdge(m_isf->output[0], m_screen->input[0]);

    // Edges from image nodes to image inputs
    int image_i = 0;
    int i = 0;
    for(const isf::input& input : m_program.descriptor.inputs)
    {
      auto node = ossia::visit(PreviewInputVisitor{image_i}, input.data);
      if(node)
      {
        m_graph.addNode(node);

        m_graph.addEdge(node->output[0], m_isf->input[i]);
        m_previewEdges.emplace_back(node->output[0], m_isf->input[i]);

        m_textures.push_back(std::unique_ptr<score::gfx::Node>(node));
      }
      i++;
    }

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

  std::unique_ptr<score::gfx::ScreenNode> m_screen{};
private:
  std::unique_ptr<score::gfx::ISFNode> m_isf{};
  std::vector<std::unique_ptr<score::gfx::Node>> m_textures;
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
  g_shaderPreviewScheduledForDeletion = true;
  QTimer::singleShot(std::chrono::seconds(5), qApp, []() {
    if(g_shaderPreviewScheduledForDeletion)
    {
      delete g_shaderPreview;
      g_shaderPreview = nullptr;
      g_shaderPreviewScheduledForDeletion = false;
    }
  });

  if(m_window)
    m_window->setParent(nullptr);
}

void ShaderPreviewWidget::setup()
{
  // UI setup
  auto lay = new QHBoxLayout(this);
  if((m_window = g_shaderPreview->getWindow()))
  {
    auto widg = createWindowContainer(m_window.get(), this);
    widg->setMinimumWidth(300);
    widg->setMaximumWidth(300);
    widg->setMinimumHeight(200);
    widg->setMaximumHeight(200);
    lay->addWidget(widg);
  }
  // FIXME else { display error widget }

  // so anyways, I started blasting...
  startTimer(16);
}

void ShaderPreviewWidget::timerEvent(QTimerEvent* event)
{
  if(g_shaderPreview)
  {
    g_shaderPreview->updateControls();
    g_shaderPreview->m_screen->render();
  }
}
}
