#include "SoundView.hpp"
#include <QPainter>
#include <QGraphicsView>
#include <QScrollBar>
#include <cmath>
#include <QGraphicsSceneContextMenuEvent>
#include <QTimer>
#include <score/widgets/GraphicsItem.hpp>
namespace Media
{
namespace Sound
{
LayerView::LayerView(QGraphicsItem* parent):
    Process::LayerView{parent}
  , m_cpt{new WaveformComputer{*this}}
{
    setFlag(ItemClipsToShape, true);
    this->setAcceptDrops(true);
    if(auto view = getView(*parent))
        connect(view->horizontalScrollBar(), &QScrollBar::valueChanged,
                this, &Media::Sound::LayerView::scrollValueChanged);
    connect(m_cpt, &WaveformComputer::ready,
            this, [=] (QList<QPainterPath> p, QPainterPath c, double z) {
      m_paths = std::move(p);
      m_channels = std::move(c);
      m_pathZoom = z;
      update();
    });
}

LayerView::~LayerView()
{
  m_cpt->stop();
  m_cpt->deleteLater();
}

void LayerView::setData(const MediaFileHandle& data)
{
  if(m_data)
    {
      QObject::disconnect(&m_data->decoder(), &AudioDecoder::finishedDecoding,
                          this, &LayerView::on_finishedDecoding);
      QObject::disconnect(&m_data->decoder(), &AudioDecoder::newData,
                          this, &LayerView::on_newData);
    }
    m_data = &data;
    m_numChan = data.data().size();
    if(m_data)
    {
      QObject::connect(&m_data->decoder(), &AudioDecoder::finishedDecoding,
                       this, &LayerView::on_finishedDecoding, Qt::QueuedConnection);
      QObject::connect(&m_data->decoder(), &AudioDecoder::newData,
                       this, &LayerView::on_newData, Qt::QueuedConnection);
    }
    m_sampleRate = data.sampleRate();
    m_cpt->dirty = true;
}

void LayerView::recompute(ZoomRatio ratio)
{
  m_zoom = ratio;
  if(m_data)
  {
    m_cpt->recompute(m_data.data(), ratio);
  }
}


void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
  emit askContextMenu(event->screenPos(), event->scenePos());

  event->accept();
}

void LayerView::paint_impl(QPainter* painter) const
{
    painter->setRenderHint(QPainter::Antialiasing, false);
    const int nchannels = m_numChan;
    if (nchannels == 0)
        return;

    painter->setBrush(Qt::darkCyan);
    painter->setPen(Qt::darkBlue);

    painter->save();

    for (const auto& path : m_paths)
        painter->drawPath(path);

    const auto h = -height() / nchannels;
    const auto dblh = 2. * h;

    painter->scale(1, -1);
    painter->translate(0, h + 1);

    for (const auto& path : m_paths) {
        painter->drawPath(path);
        painter->translate(0., dblh + 1);
    }

    painter->restore();

    painter->setPen(Qt::lightGray);
    painter->drawPath(m_channels);
}

void LayerView::scrollValueChanged(int sbvalue) {
  recompute(m_zoom);
}

void LayerView::on_finishedDecoding()
{
  m_cpt->dirty = true;
  recompute(m_zoom);
}

void LayerView::on_newData()
{
  m_cpt->dirty = true;
  recompute(m_zoom);
}

void LayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  emit pressed(ev->scenePos());
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
    emit dropReceived(event->pos(), *event->mimeData());
}




WaveformComputer::WaveformComputer(LayerView& layer)
  : m_layer{layer}
{
  connect(this, &WaveformComputer::recompute,
          this, &WaveformComputer::on_recompute, Qt::QueuedConnection);

  this->moveToThread(&m_drawThread);
  m_drawThread.start();
}


WaveformComputer::action WaveformComputer::compareDensity(const double density) {
  int ratio{};
  if(density > m_density)
  {
    ratio = 2;
  }
  else
  {
    ratio = 8;
  }
  if (dirty
      || m_density == -1
      || m_density >= 2 * ratio * density
      || 2 * ratio * m_density <= density
      || (int)m_layer.width() == 0
      || m_curdata.empty()) {
    dirty = false;
    return RECOMPUTE_ALL;
  }
  if (m_density >= ratio * density) {
    return USE_NEXT;
  }
  if (ratio * m_density <= density) {
    return USE_PREV;
  }
  return KEEP_CUR;
}


void WaveformComputer::computeDataSet(
    const MediaFileHandle& data,
    ZoomRatio ratio,
    double* densityptr,
    std::vector<std::vector<float> >& dataset)
{
  auto& arr = data.data();

  const int nchannels = data.channels();
  const int density = std::max((data.sampleRate() * ratio) / 1000., 1.);

  if (densityptr != nullptr)
    *densityptr = density;

  dataset.resize(nchannels);
  for (int c = 0; c < nchannels; ++c) {
    const auto& chan = arr[c];
    const int chan_n = std::min(data.decoder().decoded, chan.size());

    const double length = double(1000ll * chan_n) / data.sampleRate(); // duration of the track
    const double size = ratio > 0 ? length / ratio : 0; // number of pixels the track will occupy in its entirety

    const int npoints = size;
    std::vector<float>& rmsv = dataset[c];
    rmsv.resize(npoints);

    const float one_over_dens = 1. / density;
    for (int i = 0; i < npoints; ++i)
    {
      rmsv[i] = 0;
      for (int j = 0;
           (j < density) && ((i * density + j) < chan_n);
           ++j)
      {
        auto s = chan[i * density + j];
        rmsv[i] += s * s;
      }
    }

    for(int i = 0; i < npoints; i++)
    {
      rmsv[i] = std::sqrt(rmsv[i] * one_over_dens);
    }
  }
}


void WaveformComputer::drawWaveForms(const MediaFileHandle& data, ZoomRatio ratio)
{
  QList<QPainterPath> paths;
  QPainterPath channels;

  auto& arr = data.data();
  const double density = std::max((ratio * data.sampleRate()) / 1000., 1.);
  const double densityratio = (m_density > 0 && density > 0) ? m_density / density : 1.;

  int nchannels = arr.size();
  if (nchannels == 0)
    return;

  // Height of each channel
  const auto h = m_layer.height() / (double)nchannels;

  const int64_t w = m_layer.width();

  // Trace lines between channels

  for (int c = 1 ; c < nchannels; ++c) {
    channels.moveTo(0, c * h);
    channels.lineTo(w, c * h);
  }

  // Get horizontal offset
  auto view = getView(m_layer);
  if(!view)
    return;
  auto x0 = std::max(m_layer.mapFromScene(view->mapToScene(0, 0)).x(), qreal(0));

  int64_t i0 = x0 / densityratio;
  const int64_t n = m_curdata[0].size();
  if(n == 0)
    return;

  auto xf = m_layer.mapFromScene(view->mapToScene(view->width(), 0)).x();

  for (int64_t c = 0; c < nchannels ; ++c) {
    const int64_t current_height = c * h;
    const std::vector<float>& dataset = m_curdata[c];

    QPainterPath path{};
    path.setFillRule(Qt::WindingFill);

    // Draw path for current channel

    const float half_h = h / 2.f;
    const float height_adjustemnt = current_height + half_h;
    if (n > i0) {
      path.moveTo(x0, double(dataset[i0] + height_adjustemnt));
      double x = x0;
      for (int64_t i = i0; (i < n) && (x <= xf); ++i) {
        x = i * densityratio;
        path.lineTo(x, double(dataset[i] * half_h + height_adjustemnt));
      }
      path.lineTo(x, height_adjustemnt);
    }
    paths.push_back(std::move(path));
  }

  emit ready(std::move(paths), std::move(channels), ratio);
}


void WaveformComputer::on_recompute(const MediaFileHandle* pdata, ZoomRatio ratio)
{
  auto& data = *pdata;
  m_zoom = ratio;

  if(data.channels() == 0)
    return;

  const int64_t density = std::max((int)(ratio * data.sampleRate() / 1000), 1);
  long action = compareDensity(density);

  switch (action)
  {
    case KEEP_CUR:
      break;
    case USE_NEXT:
      std::swap(m_prevdata, m_curdata);
      std::swap(m_curdata, m_nextdata);
      m_prevdensity = m_density;
      m_density = m_nextdensity;

      QTimer::singleShot(0, [this,&data,ratio,density] {
        if (density > 1)
          computeDataSet(data, ratio / 2., &m_nextdensity, m_nextdata);
        else
          m_nextdata = m_curdata;
      });
      break;
    case USE_PREV:
      std::swap(m_nextdata, m_curdata);
      std::swap(m_curdata, m_prevdata);
      m_nextdensity = m_density;
      m_density = m_prevdensity;
      QTimer::singleShot(0, [this,&data,ratio] {
        computeDataSet(data, 2. * ratio, &m_prevdensity, m_prevdata);
      });
      break;
    case RECOMPUTE_ALL:
      computeDataSet(data, ratio, &m_density, m_curdata);
      QTimer::singleShot(0, [this,&data,ratio] {
        computeDataSet(data, 2. * ratio, &m_prevdensity, m_prevdata);
        computeDataSet(data, ratio / 2., &m_nextdensity, m_nextdata);
      });
      break;
    default:
      break;
  }

  drawWaveForms(data, ratio);
}

}
}
