#include "SoundView.hpp"
#include <Media/Sound/SoundModel.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/RMSData.hpp>

#include <score/tools/std/Invoke.hpp>
#include <score/tools/ThreadPool.hpp>

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
  if (auto view = getView(*parent))
  {
    connect(
        view->horizontalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        &Media::Sound::LayerView::scrollValueChanged);
  }
  connect(m_cpt, &WaveformComputer::ready,
          this, [=](QVector<QImage*> img, ComputedWaveform wf) {
    {
      QImagePool::instance().giveBack(m_images);
      m_images = std::move(img);

      // We display the image at the device ratio of the view
      auto view = ::getView(*this);
      if (view)
      {
        for (auto image : m_images)
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
  ossia::qt::run_async(m_cpt, &QObject::deleteLater);
  m_cpt = nullptr;

  score::ThreadPool::instance().releaseThread();

  QImagePool::instance().giveBack(m_images);
}

void LayerView::setData(const std::shared_ptr<AudioFile>& data)
{
  if (m_data)
  {
    QObject::disconnect(&m_data->rms(), nullptr, this, nullptr);
    m_data->on_finishedDecoding.disconnect<&LayerView::on_finishedDecoding>(*this);
  }

  SCORE_ASSERT(data);

  m_data = data;
  m_numChan = data->channels();
  if (m_data)
  {
    connect(
        &m_data->rms(),
        &RMSData::finishedDecoding,
        this,
        &LayerView::on_finishedDecoding,
        Qt::QueuedConnection);
    connect(&m_data->rms(), &RMSData::newData, this, &LayerView::on_newData, Qt::QueuedConnection);
    m_data->on_finishedDecoding.connect<&LayerView::on_finishedDecoding>(*this);
    on_newData();
  }
  m_sampleRate = data->sampleRate();
}

void LayerView::recompute() const
{
  if(Q_UNLIKELY(!m_data))
    return;

  if(auto view = getView(*this))
  {
    WaveformRequest req{
      m_data,
      m_zoom,
      m_tempoRatio,
      QSizeF{width(), height()},
      view->devicePixelRatioF(),
      mapFromScene(view->mapToScene(0, 0)).x(),
      mapFromScene(view->mapToScene(view->width(), 0)).x(),
      m_model.startOffset(),
      m_model.loopDuration(),
      m_model.loops(),
      m_frontColors
    };
    m_cpt->recompute(std::move(req));
  }
  m_recomputed = true;
}

void LayerView::setFrontColors(bool b)
{
  if (b != m_frontColors)
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

void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  askContextMenu(event->screenPos(), event->scenePos());

  event->accept();
}

void LayerView::paint_impl(QPainter* painter) const
{
  if (m_zoom == 0.)
    return;
  if (!m_data)
    return;

  int channels = m_images.size();
  if (channels == 0.)
  {
    if (!m_recomputed)
      recompute();
    return;
  }

  auto ratio = m_wf.zoom / m_zoom;

  const qreal w = (m_wf.xf - m_wf.x0) * ratio;
  const qreal h = height() / channels;

  const double x0 = m_wf.x0 * ratio;

  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for (int i = 0; i < channels; i++)
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
  recompute(m_zoom);
}

void LayerView::on_finishedDecoding()
{
  recompute(m_zoom);
  // qDebug() << "finished decoding ! " ;
}

void LayerView::on_newData()
{
  recompute(m_zoom);
  // qDebug() << "new data ! " ;
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

  if (event->mimeData())
    dropReceived(event->pos(), *event->mimeData());
}

void LayerView::heightChanged(qreal r)
{
  Process::LayerView::heightChanged(r);
  recompute(m_zoom);
}

void LayerView::widthChanged(qreal w)
{
  Process::LayerView::widthChanged(w);
  recompute(m_zoom);
}
}
