#include "SoundView.hpp"
#include <Media/RMSData.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/math.hpp>

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QApplication>
#include <cmath>

#include <boost/circular_buffer.hpp>
#if defined(__AVX2__) && __has_include(<immintrin.h>)
#include <immintrin.h>
#elif defined(__SSE2__) && __has_include(<xmmintrin.h>)
#include <xmmintrin.h>
#endif
#include <wobjectimpl.h>
W_REGISTER_ARGTYPE(const Media::FFMPEGAudioFileHandle*)
W_OBJECT_IMPL(Media::Sound::LayerView)
W_OBJECT_IMPL(Media::Sound::WaveformComputer)
namespace Media
{
namespace Sound
{

LayerView::LayerView(QGraphicsItem* parent)
    : Process::LayerView{parent}, m_cpt{new WaveformComputer{*this}}
{
  setCacheMode(NoCache);
  setFlag(ItemClipsToShape, true);
  this->setAcceptDrops(true);
  if (auto view = getView(*parent))
    connect(
        view->horizontalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        &Media::Sound::LayerView::scrollValueChanged);
  connect(
      m_cpt,
      &WaveformComputer::ready,
      this,
      [=](QVector<QImage> img, ComputedWaveform wf) {
        m_image = std::move(img);
        m_pixmap.resize(m_image.size() * 2);
        for(int i = 0; i < m_image.size(); i++)
        {
          m_pixmap[2 * i].convertFromImage(m_image[i]);
          m_pixmap[2 * i + 1].convertFromImage(m_image[i].mirrored(false, true));
        }
        m_wf = wf;

        update();
      });
}

LayerView::~LayerView()
{
  m_cpt->stop();
  m_cpt->moveToThread(this->thread());
  delete m_cpt;
}

void LayerView::setData(const std::shared_ptr<FFMPEGAudioFileHandle>& data)
{
  if (m_data)
  {
    QObject::disconnect(&m_data->rms(), nullptr, this, nullptr);
  }

  SCORE_ASSERT(data);

  m_data = data;
  m_numChan = data->channels();
  if (m_data)
  {
    QObject::connect(
        &m_data->rms(),
        &RMSData::finishedDecoding,
        this,
        &LayerView::on_finishedDecoding,
        Qt::QueuedConnection);
    QObject::connect(
        &m_data->rms(),
        &RMSData::newData,
        this,
        &LayerView::on_newData,
        Qt::QueuedConnection);
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
  if(m_zoom == 0.)
    return;

  int channels = m_pixmap.size();
  if(channels == 0.)
    return;

  auto ratio = m_wf.zoom / m_zoom;

  const qreal w = (m_wf.xf - m_wf.x0) * ratio;
  const qreal h = height() / channels;

  const double x0 = m_wf.x0 * ratio;

  for(int i = 0; i < channels; i++)
  {
    painter->drawPixmap(x0, h * i, w, h, m_pixmap[i]);
  }
}

void LayerView::scrollValueChanged(int sbvalue)
{
  // TODO maybe we don't actually need to always recompute... check if we're in the visible area.
  // TODO on_heightChanged
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


template <typename T = float>
struct low_pass_filter
{
  constexpr T operator()(T x, T alpha) noexcept
  {
    T hatx = alpha * x + (1.f - alpha) * hatxprev;
    hatxprev = hatx;
    xprev = x;

    return hatx;
  }

  T hatxprev{};
  T xprev{};
};

template <typename T = float, typename timestamp_t = int64_t>
struct one_euro_filter
{
  constexpr T operator()(T x) noexcept
  {
    const T dx = (x - xfilt_.xprev) * freq;
    const T edx = dxfilt_(dx, alpha(dcutoff));
    const T cutoff = mincutoff + beta * std::abs(edx);
    return xfilt_(x, alpha(cutoff));
  }

  static const constexpr float freq = 1.;
  static const constexpr float mincutoff = 0.05;
  static const constexpr float beta = 0.00002;
  static const constexpr float dcutoff = 0.005;

private:
  static constexpr T alpha(T cutoff) noexcept {
    T tau = 1.0f / (2.f * M_PI * cutoff);
    return 1.0f / (1.0f + tau / te);
  }

  static const constexpr T te = 1.0f / freq;
  low_pass_filter<T> xfilt_{}, dxfilt_{};
};

WaveformComputer::WaveformComputer(LayerView& layer) : m_layer{layer}
{
  connect(this, &WaveformComputer::recompute,
      this, [=] (const std::shared_ptr<FFMPEGAudioFileHandle>& arg_1, double arg_2) {
    int64_t n = m_redraw_count++;

    QMetaObject::invokeMethod(this, [=] { on_recompute(arg_1, arg_2, n); }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

  this->moveToThread(&m_drawThread);
  m_drawThread.start();
}

void WaveformComputer::drawWaveFormsOnImage(
    const FFMPEGAudioFileHandle& data,
    ZoomRatio ratio,
    int64_t redraw_number)
{
  int nchannels = data.channels();
  if (nchannels == 0)
    return;

  // Height of each channel
  const float h = m_layer.height() / (float)nchannels;
  const int64_t w = m_layer.width();

  // Get horizontal offset
  auto view = getView(m_layer);
  if (!view)
    return;

  // leftmost point
  double x0
      = std::max(m_layer.mapFromScene(view->mapToScene(0, 0)).x(), 0.);

  auto& rms = data.rms();

  if (rms.frames_count == 0) {
    return;
  }

  // rightmost point
  double xf = m_layer.mapFromScene(view->mapToScene(view->width(), 0)).x();

  const int64_t width = std::min(double(w), 2. * (xf - x0));

  const float half_h = h / 2;
  const int half_h_int = half_h;
  const float h_ratio = -half_h / std::numeric_limits<rms_sample_t>::max();
  float samples_per_pixels = 0.001 * ratio * data.sampleRate();
  if(samples_per_pixels <= 1e-6)
    return;

  int32_t max_pixel = std::min((int32_t)width, (int32_t) (data.decodedSamples() / samples_per_pixels));

  // TODO put in cache !
  QVector<QImage> images;
  images.resize(nchannels);
  for(QImage& image : images)
  {
    image = QImage(
          width,
          half_h,
          QImage::Format_ARGB32_Premultiplied);
  }

  constexpr const auto transparent = qRgba(0, 0, 0, 0);
  constexpr const auto orange = qRgba(250, 180, 15, 255);

  ossia::small_vector<one_euro_filter<>, 8> filter;
  filter.resize(nchannels);

  for(int32_t x = 0; x < max_pixel; x++)
  {
    if(m_redraw_count > redraw_number)
      return;

    const auto rms_sample = rms.frame(
          (x0 + x)      * samples_per_pixels,
          (x0 + x + 1.) * samples_per_pixels
          );

    for(int k = 0; k < nchannels; k++)
    {
      QImage& image = images[k];
      const int value = ossia::clamp(int(0.5 * filter[k](rms_sample[k]) * h_ratio + half_h), 0, half_h_int - 1);

      for(int y = 0; y < value; y++)
        image.setPixel(x, y, transparent);
      for(int y = value; y < half_h_int; y++)
        image.setPixel(x, y, orange);
    }
  }

  ComputedWaveform wf;
  wf.zoom = ratio;
  wf.x0 = x0;
  wf.xf = x0 + max_pixel;
  ready(images, wf);
}
void WaveformComputer::on_recompute(
    std::shared_ptr<FFMPEGAudioFileHandle> data_qp,
    ZoomRatio ratio,
    int64_t n)
{
  if (!data_qp)
    return;

  auto& data = *data_qp;
  m_zoom = ratio;

  if (data.channels() == 0)
    return;

  drawWaveFormsOnImage(data, ratio, n);
}
}
}

