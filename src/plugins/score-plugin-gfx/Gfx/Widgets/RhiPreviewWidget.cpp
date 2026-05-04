#include <Gfx/Widgets/RhiPreviewWidget.hpp>

#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/Node.hpp>

#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace Gfx
{
namespace
{
constexpr int kPreviewIntervalMs = 16;  // ~60 Hz
}

RhiPreviewWidget::RhiPreviewWidget(QWidget* parent)
    : QWidget{parent}
{
  // Opaque painter target: every paintEvent fully overwrites the area
  // (image blit or solid clear), so Qt can skip background fill.
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
}

RhiPreviewWidget::~RhiPreviewWidget()
{
  detach();
}

void RhiPreviewWidget::useGraph(
    score::gfx::Graph* graph,
    std::function<void(score::gfx::BackgroundNode&)> onAttached,
    std::function<void(score::gfx::BackgroundNode&)> onAboutToDetach)
{
  detach();
  m_backend = Backend::Graph;
  m_graph = graph;
  m_onAttached = std::move(onAttached);
  m_onAboutToDetach = std::move(onAboutToDetach);
  m_ctx = nullptr;
  attach();
}

void RhiPreviewWidget::useContext(GfxContext* ctx, int32_t producerNodeId)
{
  detach();
  m_backend = Backend::Context;
  m_ctx = ctx;
  m_producerNodeId = producerNodeId;
  m_graph = nullptr;
  attach();
}

void RhiPreviewWidget::setProducerNodeId(int32_t id)
{
  if(id == m_producerNodeId)
    return;

  const int32_t oldId = m_producerNodeId;
  m_producerNodeId = id;

  // Hot-rewire the producer→preview edge. Only meaningful on the
  // Context backend; the Graph backend rewires through the caller's
  // attach/detach callbacks.
  if(m_backend == Backend::Context && m_ctx
     && m_screenNodeId != score::gfx::invalid_node_index)
  {
    if(m_edgeConnected)
    {
      m_ctx->disconnect_preview_node(
          EdgeSpec{{oldId, 0}, {m_screenNodeId, 0}});
      m_edgeConnected = false;
    }
    if(m_producerNodeId != score::gfx::invalid_node_index)
    {
      m_ctx->connect_preview_node(
          EdgeSpec{{m_producerNodeId, 0}, {m_screenNodeId, 0}});
      m_edgeConnected = true;
    }
  }
}

void RhiPreviewWidget::attach()
{
  if(m_backend == Backend::None)
    return;

  m_readback = std::make_shared<QRhiReadbackResult>();

  auto node = std::make_unique<score::gfx::BackgroundNode>();
  node->shared_readback = m_readback;
  // Match the offscreen render size to the widget's pixel size; the
  // BackgroundNode allocates its own QRhi target at this size.
  const qreal dpr = devicePixelRatioF();
  const QSize px{
      qMax(1, int(width() * dpr)), qMax(1, int(height() * dpr))};
  if(width() > 0 && height() > 0)
    node->setSize(px);
  m_node = node.get();

  switch(m_backend)
  {
    case Backend::Graph: {
      if(!m_graph)
      {
        m_node = nullptr;
        return;
      }

      // Keep ownership: Graph::removeNode does not delete; we delete in
      // detach() once we've removed the node + its render list.
      m_graph->addNode(node.release());

      // The caller wires producer→preview edges here, then arranges
      // for a render list to be built (typically via createAllRenderLists).
      if(m_onAttached)
        m_onAttached(*m_node);
      break;
    }

    case Backend::Context: {
      if(!m_ctx)
      {
        m_node = nullptr;
        return;
      }

      // register_node (not register_preview_node) so that GfxContext's
      // recomputeTimers picks up BackgroundNode::configuration().
      // manualRenderingRate and drives render() automatically — the
      // BackgroundNode does its own offscreen frame + readback there.
      // We just trigger update() on the widget timer to repaint.
      m_screenNodeId = m_ctx->register_node(
          std::unique_ptr<score::gfx::Node>{node.release()});
      if(m_screenNodeId == score::gfx::invalid_node_index)
      {
        m_node = nullptr;
        return;
      }
      if(m_producerNodeId != score::gfx::invalid_node_index)
      {
        m_ctx->connect_preview_node(
            EdgeSpec{{m_producerNodeId, 0}, {m_screenNodeId, 0}});
        m_edgeConnected = true;
      }
      break;
    }

    case Backend::None:
      break;
  }

  // Single timer: refreshes the widget at preview rate. For the Graph
  // backend it also drives BackgroundNode::render() directly (the
  // manager's graph has no GfxContext timers); for the Context backend
  // GfxContext drives render() via its manual timer and we only need
  // update() here.
  if(m_timerId == 0)
    m_timerId = startTimer(kPreviewIntervalMs);
}

void RhiPreviewWidget::detach()
{
  if(m_timerId)
  {
    killTimer(m_timerId);
    m_timerId = 0;
  }

  switch(m_backend)
  {
    case Backend::Graph: {
      if(m_node && m_graph)
      {
        if(m_onAboutToDetach)
          m_onAboutToDetach(*m_node);
        m_graph->destroyOutputRenderList(*m_node);
        m_graph->removeNode(m_node);
      }
      delete m_node;
      m_node = nullptr;
      break;
    }

    case Backend::Context: {
      if(m_ctx && m_screenNodeId != score::gfx::invalid_node_index)
      {
        if(m_edgeConnected)
        {
          m_ctx->disconnect_preview_node(
              EdgeSpec{{m_producerNodeId, 0}, {m_screenNodeId, 0}});
          m_edgeConnected = false;
        }
        m_ctx->unregister_node(m_screenNodeId);
      }
      m_screenNodeId = score::gfx::invalid_node_index;
      // GfxContext owns the node lifetime via its command queue; we
      // do not delete here.
      m_node = nullptr;
      break;
    }

    case Backend::None:
      m_node = nullptr;
      break;
  }

  m_readback.reset();
}

void RhiPreviewWidget::resizeEvent(QResizeEvent* ev)
{
  QWidget::resizeEvent(ev);
  if(m_node)
  {
    const qreal dpr = devicePixelRatioF();
    const QSize px{
        qMax(1, int(ev->size().width() * dpr)),
        qMax(1, int(ev->size().height() * dpr))};
    m_node->setSize(px);
  }
}

void RhiPreviewWidget::timerEvent(QTimerEvent* ev)
{
  if(ev->timerId() != m_timerId)
  {
    QWidget::timerEvent(ev);
    return;
  }

  // Graph backend: drive the offscreen frame + readback ourselves
  // (the manager's private graph has no timer infrastructure).
  // Context backend: GfxContext drives render() via its manual timer.
  if(m_backend == Backend::Graph && m_node)
    m_node->render();

  update();
}

void RhiPreviewWidget::paintEvent(QPaintEvent*)
{
  QPainter painter{this};

  if(m_readback)
  {
    const auto& rb = *m_readback;
    const int w = rb.pixelSize.width();
    const int h = rb.pixelSize.height();
    const int expected = w * h * 4;
    if(w > 0 && h > 0 && rb.data.size() >= expected)
    {
      QImage img{
          reinterpret_cast<const unsigned char*>(rb.data.constData()),
          w, h, w * 4, QImage::Format_RGBA8888};
      painter.drawImage(rect(), img);
      return;
    }
  }

  painter.fillRect(rect(), Qt::black);
}
}
