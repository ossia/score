/*
#include <Spline3D/Widget.hpp>
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhi_p_p.h>
#include <QtGui/private/qrhigles2_p.h>
#include <QVBoxLayout>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include <Gfx/Graph/window.hpp>
#include <Gfx/Graph/mesh.hpp>

namespace Spline3D
{
class MultipassNode : NodeModel {

  struct Pass {
    Mesh* mesh{};
    QShader vertexShader;
    QShader fragmentShader;
    std::unique_ptr<char[]> material;
  };

  std::vector<Pass> m_passes;
};

struct RenderedMultipassNode : RenderedNode
{
  struct Pass {
    Mesh* mesh{};
    QRhiBuffer* meshBuffer{};
    QRhiBuffer* idxBuffer{};
    QRhiRenderPassDescriptor* rp{};
    QRhiGraphicsPipeline* pipeline{};
    QRhiShaderResourceBindings* srb{};
  };
  std::vector<Pass> m_passes;

  void customInit(Renderer& renderer) override
  {
    auto& n = (MultipassNode&)(node);

  }

  void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
  {
  }

  void runPass(Renderer& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch& res) override
  {
    update(renderer, res);

    auto updateBatch = &res;

    // Draw the passes
    std::size_t k = 0;
    for(const auto& pass : m_passes)
    {
      SCORE_ASSERT(pass.pipeline);
      SCORE_ASSERT(pass.srb);
      // TODO : combine all the uniforms..

      // TODO need to free stuff
      cb.beginPass(m_rt.renderTarget, Qt::black, {1.0f, 0}, updateBatch);
      {
        cb.setGraphicsPipeline(pass.pipeline);
        cb.setShaderResources(pass.srb);

        const auto sz = renderer.state.size;
        cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

        node.mesh().setupBindings(*pass.meshBuffer, pass.idxBuffer, cb);

        cb.draw(pass.mesh->vertexCount);
      }

      cb.endPass();

      k++;

       if(k < m_passes.size())
       {
         // Not the last pass: we have to use another resource batch
         updateBatch = renderer.state.rhi->nextResourceUpdateBatch();
       }
    }
  }

  void customRelease(Renderer& renderer) override
  {
    for (auto& pass : m_passes)
    {
      // pass.rt->releaseAndDestroyLater();
      pass.rp->releaseAndDestroyLater();
      // pass.texture->releaseAndDestroyLater();
      // pass.processUBO->releaseAndDestroyLater();
      pass.srb->releaseAndDestroyLater();
      pass.pipeline->releaseAndDestroyLater();
    }

    m_passes.clear();
  }
};

Widget::Widget()
{
  auto lay = new QVBoxLayout{this};

  // Create nodes
  auto window = new ScreenNode{true};
  m_graph.addNode(window);


  // Create edges

  // Setup renderers

  m_graph.setupOutputs(GraphicsApi::OpenGL);

  SCORE_ASSERT(window->window);
  auto widg = createWindowContainer(window->window.get(), this);
  widg->setMinimumWidth(300);
  widg->setMaximumWidth(300);
  widg->setMinimumHeight(200);
  widg->setMaximumHeight(200);
  lay->addWidget(widg);

  //startTimer(16);
}

Widget::~Widget()
{

}

}
*/
