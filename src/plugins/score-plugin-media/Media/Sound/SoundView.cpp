#include "SoundView.hpp"
#include <Media/RMSData.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Invoke.hpp>

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
        m_pixmap.resize(img.size() * 2);
        for(int i = 0; i < img.size(); i+=2)
        {
          m_pixmap[i].convertFromImage(img[i]);
          m_pixmap[i+1].convertFromImage(img[i].mirrored(false, true));
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
  for(int i = 0; i < m_pixmap.size(); i++)
  {
    double x0 = m_wf.x0 * ratio;
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

WaveformComputer::WaveformComputer(LayerView& layer) : m_layer{layer}
{
  connect(this, &WaveformComputer::recompute,
      this, [=] (const std::shared_ptr<FFMPEGAudioFileHandle>& arg_1, double arg_2) {
    int64_t n = m_redraw_count;
    n++;
    m_redraw_count = n;

    QMetaObject::invokeMethod(this, [=] { on_recompute(arg_1, arg_2, n); }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

  this->moveToThread(&m_drawThread);
  m_drawThread.start();
}

void WaveformComputer::drawWaveFormsOnImage(
    const FFMPEGAudioFileHandle& data,
    ZoomRatio ratio,
    QImage& img,
    int64_t redraw_number)
{
//  QPainter p(img);
  //p.setPen(Qt::green);

  int nchannels = data.channels();
  if (nchannels == 0)
    return;

  // Height of each channel
  const auto h = m_layer.height() / (double)nchannels;
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

  qDebug() << rms.frames_count;
  // rightmost point
  double xf = m_layer.mapFromScene(view->mapToScene(view->width(), 0)).x();


  const int64_t width = std::min(double(w), 2. * (xf - x0));

  const auto half_h = h / 2;
  const auto h_ratio = -half_h / std::numeric_limits<rms_sample_t>::max();
  double samples_per_pixels = 0.001 * ratio * data.sampleRate();
  if(samples_per_pixels <= 1e-6)
    return;

  int32_t max_pixel = std::min((int32_t)width, (int32_t) (data.decoder().decoded / samples_per_pixels));

  // TODO put in cache !
  QVector<QImage> images;
  images.resize(data.channels());
  for(QImage& image : images)
  {
    image = QImage(
          width,
          half_h,
          QImage::Format_ARGB32_Premultiplied);
  }

  constexpr auto f = 1.;//std::numeric_limits<rms_sample_t>::max() / 2;

  decltype(rms.frame(0, 0)) prev;
  prev.resize(data.channels());
  for(int32_t x = 0; x < max_pixel; x++)
  {
    if(m_redraw_count > redraw_number)
      return;

    const auto rms_sample = rms.frame(
          (x0 + x)      * samples_per_pixels,
          (x0 + x + 1.) * samples_per_pixels);
    //qDebug() << x << rms_sample[0];

    for(int k = 0; k < data.channels(); k++)
    {
      auto& image = images[k];
      const int value = rms_sample[k] * h_ratio / f + half_h;

      for(int y = 0; y < value; y++) {
        image.setPixel(x, y, qRgba(0, 0, 0, 0));
      }
      for(int y = value; y < half_h; y++) {
        image.setPixel(x, y, qRgba(250, 180, 15, 255));
      }
    }
    prev = rms_sample;
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

  if (!data.handle())
    return;
  if (data.channels() == 0)
    return;

  QImage img;
  drawWaveFormsOnImage(*data_qp, ratio, img, n);
}
}
}

