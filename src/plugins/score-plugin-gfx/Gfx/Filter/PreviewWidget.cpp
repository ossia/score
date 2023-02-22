#include <Process/Preset.hpp>

#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>

#include <ossia/network/value/value.hpp>

namespace Gfx
{
namespace
{
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
};
}

ShaderPreviewWidget::ShaderPreviewWidget(const QString& path, QWidget* parent)
    : QWidget{parent}
{
  ShaderSource program = programFromFragmentShaderPath(path, {});

  score::gfx::GraphicsApi api = score::gfx::GraphicsApi::Vulkan;
  QShaderVersion version = QShaderVersion(100);
  if(const auto& [processed, error]
     = ProgramCache::instance().get(api, version, program);
     bool(processed))
  {
    m_program = *processed;
    setup();
  }
}

ShaderPreviewWidget::ShaderPreviewWidget(const Process::Preset& preset, QWidget* parent)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();
  if(!obj.HasMember("Fragment") || !obj.HasMember("Vertex"))
    return;
  auto frag = obj["Fragment"].GetString();
  auto vert = obj["Vertex"].GetString();
  ShaderSource program{vert, frag};

  score::gfx::GraphicsApi api = score::gfx::GraphicsApi::Vulkan;
  QShaderVersion version = QShaderVersion(100);
  if(const auto& [processed, error]
     = ProgramCache::instance().get(api, version, program);
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

ShaderPreviewWidget::~ShaderPreviewWidget() { }

void ShaderPreviewWidget::setup()
{
  // Create our graph
  m_isf = std::make_unique<score::gfx::ISFNode>(
      m_program.descriptor, m_program.vertex, m_program.fragment);
  m_screen = std::make_unique<score::gfx::ScreenNode>(true);
  auto window = m_screen.get();

  m_graph.addNode(m_isf.get());
  m_graph.addNode(window);

  // Edge from filter to output
  m_graph.addEdge(m_isf->output[0], window->input[0]);

  // Edges from image nodes to image inputs
  int image_i = 0;
  int i = 0;
  for(const isf::input& input : m_program.descriptor.inputs)
  {
    auto node = ossia::visit(PreviewInputVisitor{image_i}, input.data);
    if(node)
    {
      m_graph.addNode(node);
      m_previewInputs.push_back(node);

      m_graph.addEdge(node->output[0], m_isf->input[i]);

      m_textures.push_back(std::unique_ptr<score::gfx::Node>(node));
    }
    i++;
  }

  m_graph.createAllRenderLists(score::gfx::GraphicsApi::OpenGL);

  // UI setup
  auto lay = new QHBoxLayout(this);
  const auto& w = window->window();
  SCORE_ASSERT(w);
  auto widg = createWindowContainer(w.get(), this);
  widg->setMinimumWidth(300);
  widg->setMaximumWidth(300);
  widg->setMinimumHeight(200);
  widg->setMaximumHeight(200);
  lay->addWidget(widg);

  // so anyways, I started blasting...
  startTimer(16);
}

void ShaderPreviewWidget::timerEvent(QTimerEvent* event)
{
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
}
