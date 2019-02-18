#include "SoundView.hpp"

#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/std/Invoke.hpp>
#include <score/tools/Todo.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <cmath>

#if defined(__AVX2__) && __has_include(<immintrin.h>)
#include <immintrin.h>
#elif defined(__SSE2__) && __has_include(<xmmintrin.h>)
#include <xmmintrin.h>
#endif
#include <wobjectimpl.h>
W_REGISTER_ARGTYPE(const Media::MediaFileHandle*)
W_OBJECT_IMPL(Media::Sound::LayerView)
W_OBJECT_IMPL(Media::Sound::WaveformComputer)
namespace Media
{
namespace Sound
{
/*
QImage render(const QList<QPainterPath>& paths, const QPainterPath& m_channels, QRectF rect, double height, double zoom)
{
  auto image = QImage(rect.width(), rect.height(), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  {
    auto painter = std::make_unique<QPainter>(&image);
    painter->setRenderHint(QPainter::Antialiasing, false);
    const int nchannels = paths.size();
    if (nchannels == 0)
      return {};

    painter->setBrush(Qt::darkCyan);
    painter->setPen(Qt::darkBlue);

    painter->save();

    painter->scale(zoom, 1.);

    for (const auto& path : paths)
      painter->drawPath(path);

    const auto h = -height / nchannels;
    const auto dblh = 2. * h;

    painter->scale(1., -1);
    painter->translate(0, h + 1);

    for (const auto& path : paths)
    {
      painter->drawPath(path);
      painter->translate(0., dblh + 1);
    }

    painter->restore();

    painter->setPen(Qt::lightGray);
    painter->drawPath(m_channels);
  }

  return image;
}
*/
LayerView::LayerView(QGraphicsItem* parent)
    : Process::LayerView{parent}, m_cpt{new WaveformComputer{*this}}
{
  setFlag(ItemClipsToShape, true);
  this->setAcceptDrops(true);
  if (auto view = getView(*parent))
    connect(
        view->horizontalScrollBar(), &QScrollBar::valueChanged, this,
        &Media::Sound::LayerView::scrollValueChanged);
  connect(
      m_cpt, &WaveformComputer::ready, this,
      [=](QList<QPainterPath> p, QPainterPath c, double z, QImage img) {
        m_paths = std::move(p);
        m_channels = std::move(c);
        m_pathZoom = z;

        update();
      });
}

LayerView::~LayerView()
{
  m_cpt->stop();
  m_cpt->moveToThread(this->thread());
  delete m_cpt;
}

void LayerView::setData(const std::shared_ptr<MediaFileHandle>& data)
{
  if (m_data)
  {
    QObject::disconnect(
        &m_data->decoder(), &AudioDecoder::finishedDecoding, this,
        &LayerView::on_finishedDecoding);
    QObject::disconnect(
        &m_data->decoder(), &AudioDecoder::newData, this,
        &LayerView::on_newData);
  }

  SCORE_ASSERT(data);

  m_data = data;
  m_numChan = data->data().size();
  if (m_data)
  {
    QObject::connect(
        &m_data->decoder(), &AudioDecoder::finishedDecoding, this,
        &LayerView::on_finishedDecoding, Qt::QueuedConnection);
    QObject::connect(
        &m_data->decoder(), &AudioDecoder::newData, this,
        &LayerView::on_newData, Qt::QueuedConnection);
  }
  m_sampleRate = data->sampleRate();
  m_cpt->dirty = true;
}

void LayerView::recompute(ZoomRatio ratio)
{
  m_zoom = ratio;
  if (m_data)
  {
    m_cpt->recompute(m_data, ratio);
  }
}

void LayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  askContextMenu(event->screenPos(), event->scenePos());

  event->accept();
}

void LayerView::paint_impl(QPainter* painter) const
{
//  painter->drawPixmap(0, 0, m_pixmap);
  painter->setRenderHint(QPainter::Antialiasing, false);
  const int nchannels = m_numChan;
  if (nchannels == 0)
    return;

  painter->setBrush(Qt::darkCyan);
  painter->setPen(Qt::darkBlue);

  painter->save();

  painter->scale(m_pathZoom / m_zoom, 1.);

  for (const auto& path : m_paths)
    painter->drawPath(path);

  const auto h = -height() / nchannels;
  const auto dblh = 2. * h;

  painter->scale(1., -1);
  painter->translate(0, h + 1);

  for (const auto& path : m_paths)
  {
    painter->drawPath(path);
    painter->translate(0., dblh + 1);
  }

  painter->restore();

  painter->setPen(Qt::lightGray);
  painter->drawPath(m_channels);
}

void LayerView::scrollValueChanged(int sbvalue)
{
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

WaveformComputer::WaveformComputer(LayerView& layer) : m_layer{layer}
{
  connect(
      this, &WaveformComputer::recompute, this,
      &WaveformComputer::on_recompute, Qt::QueuedConnection);

  this->moveToThread(&m_drawThread);
  m_drawThread.start();
}

WaveformComputer::action WaveformComputer::compareDensity(const double density)
{
  int ratio{};
  if (density > m_density)
  {
    ratio = 2;
  }
  else
  {
    ratio = 8;
  }
  if (dirty || m_density == -1 || m_density >= 2 * ratio * density
      || 2 * ratio * m_density <= density || (int)m_layer.width() == 0
      || m_curdata.empty())
  {
    dirty = false;
    return RECOMPUTE_ALL;
  }
  if (m_density >= ratio * density)
  {
    return USE_NEXT;
  }
  if (ratio * m_density <= density)
  {
    return USE_PREV;
  }
  return KEEP_CUR;
}

void WaveformComputer::computeDataSet(
    const MediaFileHandle& data, ZoomRatio ratio, double* densityptr,
    std::vector<ossia::float_vector>& dataset)
{
  if (!data.handle())
    return;

  auto& arr = data.data();

  if(ratio < 0.)
    ratio = 0.;
  const std::size_t nchannels = data.channels();
  const std::size_t density = std::max((data.sampleRate() * ratio) / 1000., 1.);

  if (densityptr != nullptr)
    *densityptr = density;

  dataset.resize(nchannels);
  for (std::size_t c = 0; c < nchannels; ++c)
  {
    const auto& chan = arr[c];
    const std::size_t chan_n = std::min(data.decoder().decoded, chan.size());

    const double length
        = double(1000ll * chan_n) / data.sampleRate(); // duration of the track
    const std::size_t npoints
        = ratio > 0
              ? length / ratio
              : 0ul; // number of pixels the track will occupy in its entirety

    ossia::float_vector& rmsv = dataset[c];
    rmsv.resize(npoints);

    const float one_over_dens = 1.f / density;
    for (std::size_t i = 0; i < npoints; ++i)
    {
      rmsv[i] = 0;
      const std::size_t i_dense = i * density;
      const std::size_t max_a = density - 1;
      const std::size_t max_b = chan_n - i_dense - 1;
      const std::size_t limit = std::min(max_a, max_b);
      const double* chan_start = &chan[i_dense];
      for (std::size_t j = 0; j < limit; ++j)
      {
        float s = *(chan_start + j);
        rmsv[i] += s * s;
      }
    }

    std::size_t i = 0;
#if defined(__AVX512__)
    if (npoints > 8)
    {
      for (; i < npoints - 8; i += 8)
      {
        const auto one_over_dens_avx = _mm256_set1_ps(one_over_dens);
        __m256 X = _mm256_mul_ps(
            _mm256_load_ps(&rmsv[i]), one_over_dens_avx);
        _mm256_store_ps(&rmsv[i], _mm256_mul_ps(_mm256_rsqrt14_ps(X), X));
      }
    }
#elif defined(__SSE2__)
    if (npoints >= 4)
    {
      const auto one_over_dens_sse = _mm_set1_ps(one_over_dens);
      for (; i < npoints - 4; i += 4)
      {
        float* addr = &rmsv[i];
        __m128 X
            = _mm_mul_ps(_mm_load_ps(addr), one_over_dens_sse);
        _mm_store_ps(addr, _mm_mul_ps(X, _mm_rsqrt_ps(X)));
      }
    }
#endif

    for (; i < npoints; i++)
      rmsv[i] = std::sqrt(rmsv[i] * one_over_dens);
  }
}

void WaveformComputer::drawWaveForms(
    const MediaFileHandle& data, ZoomRatio ratio)
{
  QList<QPainterPath> paths;
  QPainterPath channels;

  auto& arr = data.data();
  const double density = std::max((ratio * data.sampleRate()) / 1000., 1.);
  const double densityratio
      = (m_density > 0 && density > 0) ? m_density / density : 1.;

  int nchannels = arr.size();
  if (nchannels == 0)
    return;
  if(m_curdata.size() < nchannels)
    return;

  // Height of each channel
  const auto h = m_layer.height() / (double)nchannels;

  const int64_t w = m_layer.width();

  // Trace lines between channels

  for (int c = 1; c < nchannels; ++c)
  {
    channels.moveTo(0, c * h);
    channels.lineTo(w, c * h);
  }

  // Get horizontal offset
  auto view = getView(m_layer);
  if (!view)
    return;
  auto x0
      = std::max(m_layer.mapFromScene(view->mapToScene(0, 0)).x(), qreal(0));

  int64_t i0 = x0 / densityratio;
  const int64_t n = m_curdata[0].size();
  if (n == 0)
    return;

  auto xf = m_layer.mapFromScene(view->mapToScene(view->width(), 0)).x();

  const float half_h = h / 2.f;
  for (int64_t c = 0; c < nchannels; ++c)
  {
    const int64_t current_height = c * h;
    const ossia::float_vector& dataset = m_curdata[c];

    QPainterPath path{};
    path.setFillRule(Qt::WindingFill);

    // Draw path for current channel
    const float height_adj = current_height + half_h;
    if (n > i0)
    {
      path.moveTo(x0, double(dataset[i0] + height_adj));
      double x = x0;
      for (int64_t i = i0; (i < n) && (x <= xf); ++i)
      {
        x = i * densityratio;
        path.lineTo(x, double(dataset[i] * half_h + height_adj));
      }
      path.lineTo(x, height_adj);
    }
    paths.push_back(std::move(path));
  }

  ready(std::move(paths), std::move(channels), ratio, QImage());
}

void WaveformComputer::on_recompute(
    std::shared_ptr<MediaFileHandle> data_qp, ZoomRatio ratio)
{
  if (!data_qp)
    return;

  auto& data = *data_qp;
  m_zoom = ratio;

  if (!data.handle())
    return;
  if (data.channels() == 0)
    return;

  const int64_t density = std::max((int64_t)(ratio * data.sampleRate() / 1000ll), (int64_t)1);
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

     score::invoke([p=QPointer{this}, data_qp, ratio, density] {
       if (!data_qp || !p)
          return;
        if (density > 1)
          p->computeDataSet(*data_qp, ratio / 2., &p->m_nextdensity, p->m_nextdata);
        else
          p->m_nextdata = p->m_curdata;
      });
      break;
    case USE_PREV:
      std::swap(m_nextdata, m_curdata);
      std::swap(m_curdata, m_prevdata);
      m_nextdensity = m_density;
      m_density = m_prevdensity;
      score::invoke([p=QPointer{this}, data_qp, ratio] {
        if (!data_qp || !p)
          return;
        p->computeDataSet(*data_qp, 2. * ratio, &p->m_prevdensity, p->m_prevdata);
      });
      break;
    case RECOMPUTE_ALL:
      computeDataSet(data, ratio, &m_density, m_curdata);
      score::invoke([p=QPointer{this}, data_qp, ratio] {
        if (!data_qp || !p)
          return;
        p->computeDataSet(*data_qp, 2. * ratio, &p->m_prevdensity, p->m_prevdata);
        p->computeDataSet(*data_qp, ratio / 2., &p->m_nextdensity, p->m_nextdata);
      });
      break;
    default:
      break;
  }

  drawWaveForms(data, ratio);
}
}
}
