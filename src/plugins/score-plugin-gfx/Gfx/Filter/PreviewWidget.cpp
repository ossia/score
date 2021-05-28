#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>

namespace Gfx
{
namespace
{
struct PreviewInputvisitor
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
    static std::array<score::gfx::Image, 3> images{
        score::gfx::Image{
            QString{":/gfx/testcard-1.png"}, {QImage{":/gfx/testcard-1.png"}}},
        score::gfx::Image{
            QString{":/gfx/testcard-2.jpeg"},
            {QImage{":/gfx/testcard-2.jpeg"}}},
        score::gfx::Image{
            QString{":/gfx/testcard-3.png"}, {QImage{":/gfx/testcard-3.png"}}},
    };
    auto image_node = new score::gfx::ImagesNode{{images[img_count]}};
    image_node->ubo.currentImageIndex = 0;
    switch (img_count)
    {
      case 0:
        image_node->ubo.position[0] = 1.2;
        image_node->ubo.position[1] = 1.2;
        break;
      case 1:
        image_node->ubo.position[0] = 1.8;
        image_node->ubo.position[1] = 1.8;
        break;
      case 2:
        image_node->ubo.position[0] = 1.0;
        image_node->ubo.position[1] = 1.0;
        break;
    }
    image_node->ubo.scale[0] = 0.5;
    image_node->ubo.scale[1] = 0.5;

    img_count++;
    img_count = img_count % 3;
    return image_node;
  }

  score::gfx::NodeModel* operator()(const isf::audio_input& v) { return nullptr; }

  score::gfx::NodeModel* operator()(const isf::audioFFT_input& v) { return nullptr; }
};
}

ShaderPreviewWidget::ShaderPreviewWidget(const QString& path, QWidget* parent)
    : QWidget{parent}
{
  ShaderProgram program = programFromFragmentShaderPath(path, {});

  if (const auto& [processed, error] = ProgramCache::instance().get(program);
      bool(processed))
  {
    m_program = *std::move(processed);
    setup();
  }
}

void ShaderPreviewWidget::setup()
{
  // Create our graph
  m_isf = new score::gfx::ISFNode{
      m_program.descriptor,
      m_program.compiledVertex,
      m_program.compiledFragment};
  auto window = new score::gfx::ScreenNode{true};

  m_graph.addNode(m_isf);
  m_graph.addNode(window);

  // Edge from filter to output
  m_graph.addEdge(m_isf->output[0], window->input[0]);

  // Edges from image nodes to image inputs
  int image_i = 0;
  int i = 0;
  for (const isf::input& input : m_program.descriptor.inputs)
  {
    auto node = std::visit(PreviewInputvisitor{image_i}, input.data);
    if (node)
    {
      m_graph.addNode(node);
      m_previewInputs.push_back(node);

      m_graph.addEdge(node->output[0], m_isf->input[i]);
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
}
}
