#pragma once
#include <Gfx/Graph/graph.hpp>
#include <Gfx/Graph/isfnode.hpp>
#include <Gfx/Graph/node.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Gfx/Graph/shadercache.hpp>
#include <Gfx/Graph/window.hpp>
#include <Gfx/Graph/imagenode.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <QHBoxLayout>
#include <QOpenGLWidget>

namespace Gfx
{
class ShaderPreviewWidget: public QWidget
{
  Graph m_graph{};
  std::vector<NodeModel*> m_previewInputs;
  ProcessedProgram m_program;
public:
  ShaderPreviewWidget(const QString& path, QWidget* parent = nullptr): QWidget{parent}
  {
    QMetaObject::invokeMethod(
        this, [this] { startTimer(16); }, Qt::QueuedConnection);

    ShaderProgram program = programFromFragmentShaderPath(path, {});

    if(const auto& [processed, error] = ProgramCache::instance().get(program); bool(processed))
    {
      m_program = *std::move(processed);
      setup();
    }
  }

  void setup()
  {
    using namespace isf;
    struct input_vis
    {
      int& img_count;

      NodeModel* operator()(const float_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const long_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const event_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const bool_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const point2d_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const point3d_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const color_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const image_input& v)
      {
        static std::array<Gfx::Image, 3> images
        {
          Gfx::Image{ QString{":/gfx/testcard-1.png"}, QImage{":/gfx/testcard-1.png"} },
          Gfx::Image{ QString{":/gfx/testcard-1.png"}, QImage{":/gfx/testcard-2.png"} },
          Gfx::Image{ QString{":/gfx/testcard-1.png"}, QImage{":/gfx/testcard-3.png"} },
        };
        auto image_node = new ImagesNode{{images[img_count]}};
        image_node->ubo.currentImageIndex = 0;
        image_node->ubo.position[0] = 1.0;
        image_node->ubo.position[1] = 1.0;
        img_count++;
        img_count = img_count % 3;
        return image_node;
      }

      NodeModel* operator()(const audio_input& v)
      {
        return nullptr;
      }

      NodeModel* operator()(const audioFFT_input& v)
      {
        return nullptr;
      }
    };

    auto isfnode = new ISFNode{m_program.descriptor, m_program.compiledVertex, m_program.compiledFragment};
    auto window = new ScreenNode{true};

    m_graph.addNode(isfnode);
    m_graph.addNode(window);

    // Edge from filter to output
    auto e = new Edge{isfnode->output[0], window->input[0]};
    m_graph.edges.push_back(e);

    // Edges from image nodes to image inputs
    int image_i = 0;
    int i = 0;
    for (const isf::input& input : m_program.descriptor.inputs)
    {
      auto node = std::visit(input_vis{image_i}, input.data);
      if(node)
      {
        m_graph.addNode(node);
        m_previewInputs.push_back(node);

        auto e = new Edge{node->output[0], isfnode->input[i]};
        m_graph.edges.push_back(e);
      }
      i++;
    }

    m_graph.setupOutputs(GraphicsApi::OpenGL);

    auto lay = new QHBoxLayout(this);
    SCORE_ASSERT(window->window);
    auto widg = createWindowContainer(window->window.get(), this);
    widg->setMinimumWidth(300);
    widg->setMaximumWidth(300);
    widg->setMinimumHeight(200);
    widg->setMaximumHeight(200);
    lay->addWidget(widg);
  }

};
}
