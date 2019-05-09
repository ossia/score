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
#include <QDialog>
#include <QHBoxLayout>

//#include <QFormLayout>
#include <wobjectimpl.h>
#include <score/widgets/DoubleSlider.hpp>
#include <score/tools/std/Invoke.hpp>

W_REGISTER_ARGTYPE(const Media::AudioFileHandle*)
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

void LayerView::setData(const std::shared_ptr<AudioFileHandle>& data)
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
    connect(
        &m_data->rms(),
        &RMSData::finishedDecoding,
        this,
        &LayerView::on_finishedDecoding,
        Qt::QueuedConnection);
    connect(
        &m_data->rms(),
        &RMSData::newData,
        this,
        &LayerView::on_newData,
        Qt::QueuedConnection);
    on_newData();
  }
  m_sampleRate = data->sampleRate();
}

void LayerView::setFrontColors(bool b)
{
  if(b != m_frontColors)
  {
    m_frontColors = b;
    if (m_data)
    {
      m_cpt->recompute(m_data, m_zoom, m_frontColors);
    }
  }
}

void LayerView::recompute(ZoomRatio ratio)
{
  m_zoom = ratio;
  if (m_data)
  {
    m_cpt->recompute(m_data, m_zoom, m_frontColors);
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
  recompute(m_zoom);
}

void LayerView::on_newData()
{
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


WaveformComputer::WaveformComputer(LayerView& layer)
  : m_layer{layer}
  , m_view{*getView(m_layer)}
{
  connect(this, &WaveformComputer::recompute,
      this, [=] (const std::shared_ptr<AudioFileHandle>& arg_1, double arg_2, bool cols) {
    int64_t n = ++m_redraw_count;

     QMetaObject::invokeMethod(this, [=] { on_recompute(arg_1, arg_2, cols, n); }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

  this->moveToThread(&m_drawThread);
  m_drawThread.start();
}

void WaveformComputer::stop()
{
  QMetaObject::invokeMethod(this, [this] {
    moveToThread(m_layer.thread());
    m_drawThread.quit();
  });
  m_drawThread.wait();
}

void WaveformComputer::drawWaveFormsOnImage(
      const AudioFileHandle& data,
      ZoomRatio ratio,
      bool cols,
      int64_t redraw_number)
{
  int nchannels = data.channels();
  if (nchannels == 0)
    return;

  // Height of each channel
  const float h = m_layer.height() / (float)nchannels;
  if(h < 1.)
    return;
  const int32_t w = m_layer.width();
  if(w == 0)
    return;

  // leftmost point
  const int32_t x0
      = std::max(std::floor(m_layer.mapFromScene(m_view.mapToScene(0, 0)).x()), 0.);

  auto& rms = data.rms();
  if (rms.frames_count == 0)
    return;

  // rightmost point
  const auto audioSampleRate = data.sampleRate();
  // There may be a ratio because the waveform could have been computed at a different samplerate.
  const auto rate_ratio = data.rms().sampleRateRatio(audioSampleRate);
  const float samples_per_pixels = 0.001 * ratio * audioSampleRate;

  if(samples_per_pixels <= 1e-6)
    return;

  int32_t xf = std::floor(m_layer.mapFromScene(m_view.mapToScene(m_view.width(), 0)).x());
  const int32_t rightmost_sample = (int32_t) (data.decodedSamples() / samples_per_pixels);
  xf = std::min(xf, rightmost_sample);

  const int32_t width = std::min(w, (xf - x0));
  const int32_t max_pixel = std::min((int32_t)width, (int32_t) rightmost_sample);

  // height
  const float half_h = h / 2;
  const int half_h_int = half_h;
  const float h_ratio = -half_h / std::numeric_limits<rms_sample_t>::max();


  // TODO put in cache ! have some kind of rotation.
  // If we set images with the "bits" constructor it could be possible
  // to resize.
  QVector<QImage> images;
  images.resize(nchannels);
  for(QImage& image : images)
  {
    image = QImage(
          width,
          half_h,
          QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
  }

  constexpr const auto orange = qRgba(250, 180, 15, 255);
  constexpr const auto transporange = qRgba(125, 90, 7, 127);
  constexpr const auto gray = qRgba(20, 81, 120, 255);
  constexpr const auto transpgray = qRgba(5, 35, 55, 127);

  const auto main_color = cols ? orange : gray;
  const auto border_color = cols ? transporange : transpgray;
  // Draw the frames on the image
  // TODO hidpi

  for(int32_t x_samples = x0, x_pixels = x_samples - x0
      ;       x_samples < xf && x_pixels < max_pixel
      ;       x_samples++, x_pixels++
      )
  {
    if(m_redraw_count > redraw_number)
      return;

    const auto rms_sample = rms.frame(
          (x_samples)     * samples_per_pixels * rate_ratio,
          (x_samples + 1) * samples_per_pixels * rate_ratio
          );

    for(int k = 0; k < nchannels; k++)
    {
      QImage& image = images[k];
      auto dat = reinterpret_cast<uint32_t*>(image.bits());
      const int value = ossia::clamp(int(rms_sample[k] * h_ratio + half_h), 0, half_h_int - 1);

      dat[x_pixels + value * width] = border_color;

      for(int y = value +1; y < half_h_int; y++)
        dat[x_pixels + y * width] = main_color;
    }
  }

  ComputedWaveform wf;
  wf.zoom = ratio;
  wf.x0 = x0;
  wf.xf = x0 + max_pixel;
  ready(images, wf);
}
void WaveformComputer::on_recompute(
    std::shared_ptr<AudioFileHandle> data_qp,
    ZoomRatio ratio,
    bool cols,
    int64_t n)
{
  if(m_redraw_count > n)
    return;

  if (!data_qp)
    return;

  auto& data = *data_qp;
  m_zoom = ratio;

  if (data.channels() == 0)
    return;

  drawWaveFormsOnImage(data, ratio, cols, n);
}
}
}

