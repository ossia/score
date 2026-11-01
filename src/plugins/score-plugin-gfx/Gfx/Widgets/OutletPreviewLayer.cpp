#include "OutletPreviewLayer.hpp"

#include <Process/PreviewSettings.hpp>
#include <Process/Process.hpp>

#include <score/application/ApplicationContext.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/TexturePort.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>


namespace Gfx
{
OutletPreviewLayer::OutletPreviewLayer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent, std::size_t texture_outlet)
    : Process::EffectLayerView{parent}
    , m_process{process}
    , m_outlet{texture_outlet}
{
  if(auto* plug = doc.findPlugin<DocumentPlugin>())
    m_context = &plug->context;
  connect(
      &Process::PreviewSettings::instance(), &Process::PreviewSettings::enabledChanged,
      this, [this] { updateTimer(); });
  updateTimer();
}

OutletPreviewLayer::~OutletPreviewLayer()
{
  m_preview.detach();
}

void OutletPreviewLayer::updateTimer()
{
  // While the process runs, the preview refreshes at the UI event rate of the
  // settings; otherwise a slow check notices when it starts. Nothing runs
  // when previews are disabled.
  int interval = 0;
  if(Process::PreviewSettings::instance().enabled())
    interval = m_preview.attached()
                   ? std::max(1, score::AppContext().applicationSettings.uiEventRate)
                   : 500;
  if(interval == m_interval)
    return;
  if(m_timer)
    killTimer(m_timer);
  m_timer = interval > 0 ? startTimer(interval) : 0;
  m_interval = interval;
  if(interval == 0 && m_preview.attached())
  {
    m_preview.detach();
    update();
  }
}

QRectF OutletPreviewLayer::imageRect() const
{
  const auto b = boundingRect();
  const auto a = aspect ? aspect() : std::nullopt;
  if(!a || *a <= 0)
    return b;
  double w = b.width(), h = w / *a;
  if(h > b.height())
  {
    h = b.height();
    w = h * *a;
  }
  return {b.center().x() - w / 2, b.center().y() - h / 2, w, h};
}

void OutletPreviewLayer::timerEvent(QTimerEvent*)
{
  const bool was = m_preview.attached();
  refresh();
  if(was || m_preview.attached())
    update();
  updateTimer();
}

void OutletPreviewLayer::refresh()
{
  const auto* scene = this->scene();
  const bool shown = isVisible() && scene && !scene->views().empty();
  // The outlet only has a node of the graph while the process runs
  auto* outlet = m_outlet < m_process.outlets().size()
                     ? qobject_cast<TextureOutlet*>(m_process.outlets()[m_outlet])
                     : nullptr;
  const auto port = outlet ? outlet->graphicsPort() : port_index{-1, -1};
  if(!shown || !m_context || port.node == score::gfx::invalid_node_index)
  {
    if(m_preview.attached())
      m_preview.detach();
    return;
  }

  // Render at the size it is shown, in device pixels
  qreal dpr = 1.;
  if(auto* view = scene->views().front())
    dpr = view->devicePixelRatioF();
  const auto r = imageRect();
  const QSize px{std::max(16, int(r.width() * dpr)), std::max(16, int(r.height() * dpr))};
  if(!m_preview.attached())
    m_preview.attach(m_context, port, px);
  else
  {
    m_preview.setProducer(port);
    m_preview.setSize(px);
  }
}

void OutletPreviewLayer::paint_impl(QPainter* p) const
{
  const auto b = boundingRect();
  p->fillRect(b, QColor(18, 18, 20));
  const QImage img = m_preview.image();
  if(img.isNull())
  {
    QFont f = p->font();
    f.setPixelSize(9);
    p->setFont(f);
    p->setPen(QColor(170, 170, 175));
    p->drawText(b, Qt::AlignCenter, placeholder);
    return;
  }
  // The image keeps the aspect of the texture
  const auto r = TexturePreview::fit(imageRect(), img.size());
  p->setRenderHint(QPainter::SmoothPixmapTransform, true);
  p->drawImage(r, img);
  paintOverlay(p, r);
}
}
