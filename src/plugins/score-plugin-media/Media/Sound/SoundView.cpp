#include "SoundView.hpp"

#include <Media/RMSData.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/Sound/SoundModel.hpp>

#include <score/tools/ThreadPool.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/ssize.hpp>

#include <QGraphicsView>
#include <QScrollBar>

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
  connect(
      m_cpt, &WaveformComputer::ready, this,
      [=](QVector<QImage*> img, ComputedWaveform wf) {
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
  if(Q_UNLIKELY(!m_data || width() < 2. || height() < 2. || m_zoom <= 0.))
    return;

  if(auto view = getView(*this))
  {
    // By default we try to force a render of everything, but it's too slow with very large files
    double minutes
        = double(m_model.file()->decodedSamples()) / m_model.file()->sampleRate();
    if(minutes > 10)
      m_renderAll = false;

    // On the first render we render the whole thing
    double x0 = m_renderAll ? 0 : mapFromScene(view->mapToScene(0, 0)).x();
    double xf
        = m_renderAll ? 100000 : mapFromScene(view->mapToScene(view->width(), 0)).x();

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
    m_cpt->recompute(std::move(req));
    m_recomputed = true;
    m_renderAll = false;
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
    {
      m_renderAll = true;
      recompute();
    }
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

  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for(int i = 0; i < channels; i++)
  {
    painter->drawImage(QRectF{x0, h * i, w, h}, *m_images[i]);
  }
  painter->setRenderHint(QPainter::SmoothPixmapTransform, 1);
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
  recompute();
}

void LayerView::on_newData()
{
  recompute();
}

void LayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  pressed(ev->scenePos());
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
  m_renderAll = true;
  recompute();
}

void LayerView::widthChanged(qreal w)
{
  Process::LayerView::widthChanged(w);
  m_renderAll = true;
  recompute();
}
}
