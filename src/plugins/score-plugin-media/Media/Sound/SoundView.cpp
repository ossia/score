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
    // The colours are not part of the request, so the image has to be asked for
    // again even though nothing the computer reads has changed.
    m_lastRequest.reset();
    recompute();
  });

  connect(
      m_cpt, &WaveformComputer::ready, this,
      [this](QVector<QImage*> img, ComputedWaveform wf) {
    {
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

  ossia::qt::run_async(m_cpt, &QObject::deleteLater);
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
    // Render a screen's worth on either side of what is visible, so that a
    // scroll or a zoom step has something to show for the edges it exposes
    // instead of leaving them blank until the next image lands. When the whole
    // layer fits in that budget -- the case as soon as one is zoomed out -- this
    // covers all of it, and scrolling then costs nothing at all.
    //
    // The budget is capped by what the computer will rasterise: a request above
    // its ceiling is dropped and would leave the layer empty. (This used to be a
    // duration test, through a variable named `minutes` that actually held
    // seconds, so it gave up on anything longer than ten seconds.)
    const double viewWidth = view->width();
    const double dpr = view->devicePixelRatioF();
    const double physicalHeight = dpr * height() / std::max(1, m_numChan);
    const double affordable
        = physicalHeight >= 1. ? double(maxWaveformPixels) / (physicalHeight * dpr) : 0.;

    const double visibleX0 = mapFromScene(view->mapToScene(0, 0)).x();
    const double visibleXf = mapFromScene(view->mapToScene(viewWidth, 0)).x();
    const double budget = std::min(affordable, 3. * viewWidth);
    const double margin = std::max(0., (budget - (visibleXf - visibleX0)) / 2.);

    // Whole pixels: the computer floors these anyway, and leaving the raw
    // mapFromScene doubles in makes every request differ in the last bits, so
    // the identical-request check below would never fire.
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

  int channels = std::ssize(m_images);
  if(channels == 0.)
  {
    if(!m_recomputed)
      recompute();
    return;
  }

  auto ratio = m_wf.zoom / m_zoom;

  const qreal w = (m_wf.xf - m_wf.x0) * ratio;
  if(w < 2.)
    return;

  const qreal h = height() / channels;
  if(h < 2.)
    return;

  const double x0 = m_wf.x0 * ratio;

  // The image only covers the window it was asked for, so the view moving off
  // it has to be noticed. Painting is the one place that sees every way it can
  // move -- the scrollbar signal catches only some of them -- and asking for a
  // window that is already on its way costs nothing: recompute() drops a
  // request identical to the last.
  requestIfUncovered(x0, x0 + w);

  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for(int i = 0; i < channels; i++)
  {
    painter->drawImage(QRectF{x0, h * i, w, h}, *m_images[i]);
  }
  painter->setRenderHint(QPainter::SmoothPixmapTransform, 1);
}

//! Asks for a new image when [coveredX0, coveredXf) no longer holds what is
//! on screen.
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

  // A pixel of slack: both edges have been through a floor and a ceil.
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
  // Nothing in the request changes as the file decodes, but what can be drawn
  // from it does: ask again rather than recognising it as one already sent.
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
