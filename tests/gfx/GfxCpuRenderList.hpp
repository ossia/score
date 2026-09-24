#pragma once
// Drives a data-only renderer -- one that only consumes and publishes
// geometry_specs, like Merge Geometries on CPU inputs -- without a device.
//
// The RenderList is a real one whose RenderState has no QRhi, so the renderer
// under test sees "no GPU" exactly as it would at run time. The update batch
// and command buffer it is handed come from a Null QRhi: nothing is executed,
// but they are genuine objects. The geometry the renderer publishes on its
// first outlet is read back with published().

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif

#include <memory>

namespace score::test::gfx
{
class CpuRenderList
{
  struct StubOutput final : score::gfx::OutputNode
  {
    score::gfx::OutputNodeRenderer*
    createRenderer(score::gfx::RenderList&) const noexcept override
    {
      return nullptr;
    }
    void setRenderer(std::shared_ptr<score::gfx::RenderList>) override { }
    score::gfx::RenderList* renderer() const override { return nullptr; }
    void startRendering() override { }
    void render() override { }
    void stopRendering() override { }
    bool canRender() const override { return false; }
    void onRendererChange() override { }
    void createOutput(score::gfx::OutputConfiguration) override { }
    void destroyOutput() override { }
    std::shared_ptr<score::gfx::RenderState> renderState() const override
    {
      return {};
    }
    Configuration configuration() const noexcept override { return {}; }
  };

  struct SinkRenderer final : score::gfx::NodeRenderer
  {
    using NodeRenderer::NodeRenderer;
    void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
    void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
        override
    {
    }
    void release(score::gfx::RenderList&) override { }
    void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  };

  struct SinkNode final : score::gfx::Node
  {
    SinkNode()
    {
      input.push_back(
          new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
    }
    score::gfx::NodeRenderer*
    createRenderer(score::gfx::RenderList&) const noexcept override
    {
      return nullptr;
    }
  };

public:
  /// `node` must outlive this object.
  explicit CpuRenderList(score::gfx::Node& node)
      : m_rhi{[] {
        QRhiNullInitParams params;
        return QRhi::create(QRhi::Null, &params);
      }()}
      , m_list{m_output, std::make_shared<score::gfx::RenderState>()}
      , m_sink{m_sinkNode}
      , m_edge{
            node.output[0], m_sinkNode.input[0],
            Process::CableType::ImmediateGlutton}
      , renderer{node.createRenderer(m_list)}
  {
    m_sinkNode.renderedNodes[&m_list] = &m_sink;
    if(m_rhi)
    {
      m_rhi->beginOffscreenFrame(&m_cb);
      m_batch = m_rhi->nextResourceUpdateBatch();
    }
  }

  CpuRenderList(const CpuRenderList&) = delete;
  CpuRenderList& operator=(const CpuRenderList&) = delete;

  ~CpuRenderList()
  {
    if(renderer)
      renderer->release(m_list);
    renderer.reset();
    m_sinkNode.renderedNodes.clear();
    if(m_rhi)
    {
      if(m_batch)
        m_batch->release();
      m_rhi->endOffscreenFrame();
    }
  }

  /// False when no Null QRhi could be created; nothing else can be run then.
  bool valid() const noexcept { return m_rhi && m_cb && m_batch && renderer; }

  /// One frame of the renderer: update, then the initial passes that publish
  /// its output to the sink.
  void frame()
  {
    if(!m_batch)
      m_batch = m_rhi->nextResourceUpdateBatch();
    renderer->update(m_list, *m_batch, nullptr);
    renderer->runInitialPasses(m_list, *m_cb, m_batch, m_edge);
  }

  /// What the renderer last published on its first outlet, or nullptr.
  const ossia::geometry_spec* published() const
  {
    return m_sink.findGeometryByPort(0);
  }

private:
  std::unique_ptr<QRhi> m_rhi;
  StubOutput m_output;
  score::gfx::RenderList m_list;
  SinkNode m_sinkNode;
  SinkRenderer m_sink;
  score::gfx::Edge m_edge;
  QRhiCommandBuffer* m_cb{};
  QRhiResourceUpdateBatch* m_batch{};

public:
  std::unique_ptr<score::gfx::NodeRenderer> renderer;
};
}
