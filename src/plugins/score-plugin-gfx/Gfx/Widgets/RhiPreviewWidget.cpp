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
score::gfx::BackgroundNode* RhiPreviewWidget::liveNode() const noexcept
{
  switch(m_backend)
  {
    case Backend::Graph:
      return m_node;
    default:
      return nullptr;
  }
}

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
  attach();
}

void RhiPreviewWidget::useContext(GfxContext* ctx, port_index producer)
{
  detach();
  m_backend = Backend::Context;
  // Match the offscreen render size to the widget's pixel size
  const qreal dpr = devicePixelRatioF();
  const QSize px{qMax(1, int(width() * dpr)), qMax(1, int(height() * dpr))};
  m_preview.attach(ctx, producer, width() > 0 && height() > 0 ? px : QSize{});
  if(m_timerId == 0)
    m_timerId = startTimer(kPreviewIntervalMs);
}

void RhiPreviewWidget::setProducer(port_index producer)
{
  // Only meaningful on the Context backend; the Graph backend rewires through
  // the caller's attach/detach callbacks.
  if(m_backend == Backend::Context)
    m_preview.setProducer(producer);
}

void RhiPreviewWidget::attach()
{
  if(m_backend != Backend::Graph || !m_graph)
    return;

  m_readback = std::make_shared<QRhiReadbackResult>();

  // Previews are rebuilt on every selection, and a rebuild that creates its
  // own device costs a vkCreateDevice + vkDestroyDevice pair on the GUI
  // thread. Take a reference on the process-wide device cache instead.
  auto node = std::make_unique<score::gfx::BackgroundNode>(
      score::gfx::SharedDeviceMode::Cached);
  node->shared_readback = m_readback;
  // Match the offscreen render size to the widget's pixel size; the
  // BackgroundNode allocates its own QRhi target at this size.
  const qreal dpr = devicePixelRatioF();
  const QSize px{
      qMax(1, int(width() * dpr)), qMax(1, int(height() * dpr))};
  if(width() > 0 && height() > 0)
    node->setSize(px);
  m_node = node.get();

  // Keep ownership: Graph::removeNode does not delete; we delete in
  // detach() once we've removed the node + its render list.
  m_graph->addNode(node.release());

  // The caller wires producer→preview edges here, then arranges
  // for a render list to be built (typically via createAllRenderLists).
  if(m_onAttached)
    m_onAttached(*m_node);

  // Refreshes the widget at preview rate, and drives
  // BackgroundNode::render(): the manager's graph has no GfxContext timers.
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

    case Backend::Context:
      m_preview.detach();
      break;

    case Backend::None:
      m_node = nullptr;
      break;
  }

  // Forget the owners too, so a detached widget cannot outlive a graph it
  // still points at.
  m_backend = Backend::None;
  m_graph = nullptr;
  m_onAttached = {};
  m_onAboutToDetach = {};

  m_readback.reset();
}

void RhiPreviewWidget::resizeEvent(QResizeEvent* ev)
{
  QWidget::resizeEvent(ev);
  const qreal dpr = devicePixelRatioF();
  const QSize px{
      qMax(1, int(ev->size().width() * dpr)), qMax(1, int(ev->size().height() * dpr))};
  if(m_backend == Backend::Context)
    m_preview.setSize(px);
  else if(auto* node = liveNode())
    node->setSize(px);
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
  if(auto* node = liveNode(); node && m_backend == Backend::Graph)
    node->render();

  update();
}

void RhiPreviewWidget::paintEvent(QPaintEvent*)
{
  QPainter painter{this};

  QImage img;
  if(m_backend == Backend::Context)
    img = m_preview.image();
  else if(m_readback)
  {
    const auto& rb = *m_readback;
    const int w = rb.pixelSize.width();
    const int h = rb.pixelSize.height();
    if(w > 0 && h > 0 && rb.data.size() >= w * h * 4)
      img = QImage{
          reinterpret_cast<const unsigned char*>(rb.data.constData()), w, h, w * 4,
          QImage::Format_RGBA8888};
  }

  painter.fillRect(rect(), Qt::black);
  if(!img.isNull())
    painter.drawImage(TexturePreview::fit(rect(), img.size()), img);
}
}
