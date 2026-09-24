#include "SoundView.hpp"

#include <Media/RMSData.hpp>
#include <score/model/Skin.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/Sound/SoundModel.hpp>

#include <score/tools/ThreadPool.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/ssize.hpp>

#include <algorithm>
#include <cmath>

#include <QGraphicsView>
#include <QScrollBar>
#include <QTimer>

namespace Media::Sound
{
LayerView::LayerView(const ProcessModel& m, QGraphicsItem* parent)
    : Process::LayerView{parent}
    , m_cpt{new WaveformComputer{}}
    , m_model{m}
{
  setCacheMode(NoCache);
  setFlag(ItemClipsToShape, true);
  this->setAcceptDrops(true);

  if(auto view = getView(*parent))
  {
    connect(
        view->horizontalScrollBar(), &QScrollBar::valueChanged, this,
        &Media::Sound::LayerView::scrollValueChanged);
  }
  // The waveform is rasterised once and kept; a skin change is one of the
  // few things that invalidates it without the zoom or the data moving.
  score::onSkinChange(this, [this] {
    m_recomputed = false;
    // The colours are not part of the request.
    m_lastRequest.reset();
    recompute();
  });

  connect(
      m_cpt, &WaveformComputer::ready, this,
      [this](QVector<QImage*> img, ComputedWaveform wf) {
    {
      m_cpt->claim(img);
      QImagePool::instance().giveBack(m_images);
      m_images = std::move(img);

      // We display the image at the device ratio of the view
      if(auto view = ::getView(*this))
      {
        for(auto image : m_images)
        {
          image->setDevicePixelRatio(view->devicePixelRatioF());
        }
      }
    }
    m_wf = wf;

    update();
      });
  QTimer::singleShot(10, this, [this] { recompute(); });
}

LayerView::~LayerView()
{
  m_cpt->stop();

  // Not through run_async: releaseThread() below may quit the thread and drop
  // the queued call, whereas a posted DeferredDelete is still honoured.
  m_cpt->deleteLater();
  m_cpt = nullptr;

  score::ThreadPool::instance().releaseThread();

  QImagePool::instance().giveBack(m_images);
}

void LayerView::setData(const std::shared_ptr<AudioFile>& data)
{
  if(m_data)
  {
    QObject::disconnect(&m_data->rms(), nullptr, this, nullptr);
    m_data->on_finishedDecoding.disconnect<&LayerView::on_finishedDecoding>(*this);
  }

  SCORE_ASSERT(data);

  m_data = data;
  m_numChan = data->channels();
  if(m_data)
  {
    connect(
        &m_data->rms(), &RMSData::finishedDecoding, this,
        &LayerView::on_finishedDecoding, Qt::QueuedConnection);
    connect(
        &m_data->rms(), &RMSData::newData, this, &LayerView::on_newData,
        Qt::QueuedConnection);
    m_data->on_finishedDecoding.connect<&LayerView::on_finishedDecoding>(*this);
    on_newData();
  }
  m_sampleRate = data->sampleRate();
}

void LayerView::recompute() const
{
  if(Q_UNLIKELY(
         !m_data || width() < 2. || height() < 2. || m_zoom <= 0.
         || m_model.file()->sampleRate() < 1.))
    return;

  if(auto view = getView(*this))
  {
    // A screen either side of what is visible, so a scroll or a zoom step has
    // something to show for the edges it exposes. Capped by what the computer
    // will rasterise: a request above its ceiling is dropped entirely.
    const double viewWidth = view->width();
    const double dpr = view->devicePixelRatioF();
    const double physicalHeight = dpr * height() / std::max(1, m_numChan);
    const double affordable
        = physicalHeight >= 1. ? double(maxWaveformPixels) / (physicalHeight * dpr) : 0.;

    const double visibleX0 = mapFromScene(view->mapToScene(0, 0)).x();
    const double visibleXf = mapFromScene(view->mapToScene(viewWidth, 0)).x();
    const double budget = std::min(affordable, 3. * viewWidth);
    const double margin = std::max(0., (budget - (visibleXf - visibleX0)) / 2.);

    // Whole pixels, or every request differs in the last bits and the
    // identical-request check below never fires.
    const double x0 = std::floor(std::max(0., visibleX0 - margin));
    const double xf = std::ceil(std::min(width(), visibleXf + margin));

    WaveformRequest req{
        m_data,
        m_zoom,
        m_tempoRatio,
        QSizeF{width(), height()},
        view->devicePixelRatioF(),
        x0,
        xf,
        m_model.startOffset(),
        m_model.loopDuration(),
        m_model.loops(),
        m_frontColors};
    m_recomputed = true;
    if(m_lastRequest == req)
      return;

    m_lastRequest = req;
    m_cpt->recompute(std::move(req));
  }
}

void LayerView::setFrontColors(bool b)
{
  if(b != m_frontColors)
  {
    m_frontColors = b;
    recompute();
  }
}

void LayerView::setTempoRatio(double r)
{
  if(r != m_tempoRatio)
  {
    m_tempoRatio = r;
    recompute();
  }
}

void LayerView::recompute(ZoomRatio ratio)
{
  m_zoom = ratio;
  recompute();
}

void LayerView::paint_impl(QPainter* painter) const
{
  if(m_zoom == 0.)
    return;
  if(!m_data)
    return;

  const int channels = std::ssize(m_images);
  const double ratio = channels > 0 ? m_wf.zoom / m_zoom : 0.;
  const double x0 = m_wf.x0 * ratio;
  const double w = (m_wf.xf - m_wf.x0) * ratio;

  // Before the reasons not to draw: having no image, or one the zoom has
  // squeezed to nothing, is when another is most needed. Painting sees every
  // way the view can move; the scrollbar signal sees only some.
  requestIfUncovered(x0, x0 + w);

  if(channels == 0)
    return;
  if(w < 2.)
    return;

  const qreal h = height() / channels;
  if(h < 2.)
    return;

  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for(int i = 0; i < channels; i++)
  {
    painter->drawImage(QRectF{x0, h * i, w, h}, *m_images[i]);
  }
  painter->setRenderHint(QPainter::SmoothPixmapTransform, 1);
}

QRectF LayerView::renderedSpan() const noexcept
{
  if(m_images.empty() || m_zoom == 0.)
    return {};
  const double ratio = m_wf.zoom / m_zoom;
  return QRectF{m_wf.x0 * ratio, 0., (m_wf.xf - m_wf.x0) * ratio, height()};
}

//! Asks for a new image when [coveredX0, coveredXf) no longer holds the view.
void LayerView::requestIfUncovered(double coveredX0, double coveredXf) const
{
  auto view = getView(*this);
  if(!view)
    return;

  const double visibleX0
      = std::max(0., mapFromScene(view->mapToScene(0, 0)).x());
  const double visibleXf
      = std::min(width(), mapFromScene(view->mapToScene(view->width(), 0)).x());
  if(visibleXf <= visibleX0)
    return;

  // Both edges have been through a floor and a ceil.
  constexpr double slack = 1.;
  if(coveredX0 > visibleX0 + slack || coveredXf < visibleXf - slack)
    recompute();
}

void LayerView::scrollValueChanged(int sbvalue)
{
  // TODO maybe we don't actually need to always recompute... check if we're in
  // the visible area.
  // TODO on_heightChanged
  recompute();
}

void LayerView::on_finishedDecoding()
{
  // The request does not change as the file decodes, but what it draws does.
  m_lastRequest.reset();
  recompute();
}

void LayerView::on_newData()
{
  m_lastRequest.reset();
  recompute();
}

void LayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  pressed(ev->scenePos());
  ev->ignore();
}

void LayerView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void LayerView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void LayerView::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void LayerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();

  if(event->mimeData())
    dropReceived(event->pos(), *event->mimeData());
}

void LayerView::heightChanged(qreal r)
{
  Process::LayerView::heightChanged(r);
  recompute();
}

void LayerView::widthChanged(qreal w)
{
  Process::LayerView::widthChanged(w);
  recompute();
}
}
