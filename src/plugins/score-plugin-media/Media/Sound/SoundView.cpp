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
        {
          m_pixmap.resize(m_image.size());
          for(int i = 0; i < m_image.size(); i++)
            m_pixmap[i].convertFromImage(m_image[i]);
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

  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for(int i = 0; i < channels; i++)
  {
    painter->drawPixmap(x0, h * i, w, h, m_pixmap[i]);
  }
  painter->setRenderHint(QPainter::SmoothPixmapTransform, 1);
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

struct WaveformComputerImpl
{
  const AudioFileHandle& data;
  ZoomRatio ratio;
  bool cols;
  int64_t redraw_number;
  WaveformComputer& computer;

  struct SizeInfos {
    int nchannels;
    float samples_per_pixels;
    double rate_ratio;

  float h;
  int h_int;
  float h_ratio;
  float half_h;
  int half_h_int;
  float half_h_ratio;

  int32_t w;
  int32_t x0;
  int32_t xf;
  int32_t rightmost_sample;
  int32_t width;
  int32_t max_pixel;
  };

  static constexpr const auto r = qRgba(255, 0, 0, 255);
  static constexpr const auto g = qRgba(0, 255, 0, 255);
  static constexpr const auto b = qRgba(0, 0, 255, 255);
  static constexpr const auto orange = qRgba(250, 180, 15, 255);
  static constexpr const auto transporange = qRgba(125, 90, 7, 127);
  static constexpr const auto gray = qRgba(20, 81, 120, 255);
  static constexpr const auto transpgray = qRgba(5, 35, 55, 127);
  const unsigned int main_color = cols ? orange : gray;
  const unsigned int border_color = cols ? transporange : transpgray;
   QPen pen = [this] { QPen p; p.setColor(main_color); p.setCosmetic(true); p.setWidth(0); return p; }();


  struct QPainterCleanup
  {
    QPainter* p;
    int n;
    int init = 0;
    QPainterCleanup(QPainter* p, int n): p{p}, n{n}
    {
      for(int i = 0; i < n; i++)
      {
        new (&p[i]) QPainter;
      }
    }
    ~QPainterCleanup()
    {
      for(int i = 0; i < init; i++)
      {
        p[i].end();
      }

      for(int i = 0; i < n; i++)
      {
        p[i].~QPainter();
      }
    }
  };

  void compute_rms(const SizeInfos infos, const RMSData& rms)
  {
    QVector<QImage> images;
    images.resize(infos.nchannels);
    for(QImage& image : images)
    {
      image = QImage(
            infos.width,
            infos.h,
            QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
    }

    for(int32_t x_samples = infos.x0, x_pixels = x_samples - infos.x0
        ;       x_samples < infos.xf && x_pixels < infos.max_pixel
        ;       x_samples++, x_pixels++
        )
    {
      if(computer.m_redraw_count > redraw_number)
        return;

      const auto rms_sample = rms.frame(
            (x_samples)     * infos.samples_per_pixels * infos.rate_ratio,
            (x_samples + 1) * infos.samples_per_pixels * infos.rate_ratio
            );

      for(int k = 0; k < infos.nchannels; k++)
      {
        QImage& image = images[k];
        auto dat = reinterpret_cast<uint32_t*>(image.bits());

        const int value = infos.half_h_int + int(rms_sample[k] * infos.half_h_ratio);

        auto [y, end_y] =
            value < infos.half_h_int
            ? std::tuple<int, int>{value, infos.half_h_int}
            : std::tuple<int, int>{infos.half_h_int, value};

        for(; y <= end_y; y++)
        {
          dat[x_pixels + y * infos.width] = r;
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::RMS;
    result.zoom = ratio;
    result.x0 = infos.x0;
    result.xf = infos.x0 + infos.max_pixel;

    computer.ready(images, result);
  }

  bool initImages(QVector<QImage>& images, const SizeInfos& infos, QPainter* p, QPainterCleanup& _) const noexcept
  {
    images.resize(infos.nchannels);
    for(int i = 0; i < infos.nchannels; i++)
    {
      images[i] = QImage(
            infos.width,
            infos.h,
            QImage::Format_ARGB32_Premultiplied);
      images[i].fill(Qt::transparent);

      if(computer.m_redraw_count > redraw_number)
        return false;

      p[i].begin(&images[i]);
      p[i].setPen(this->pen);
      _.init++;
    }
    return true;
  }

  void compute_mean(const SizeInfos infos)
  {
    QPainter* p = (QPainter*) alloca(sizeof(QPainter) * infos.nchannels);
    pen.setColor(g);
    QVector<QImage> images;

    {
      QPainterCleanup _{p, infos.nchannels};

      ossia::small_vector<QPointF, 8> pPos(infos.nchannels);

      if(!initImages(images, infos, p, _))
        return;

      for(int32_t x_samples = infos.x0, x_pixels = x_samples - infos.x0
          ;       x_samples < infos.xf && x_pixels < infos.max_pixel
          ;       x_samples++, x_pixels++
          )
      {
        if(computer.m_redraw_count > redraw_number)
        {
          return;
        }

        const auto mean_sample = data.frame(
              (x_samples)     * infos.samples_per_pixels * infos.rate_ratio,
              (x_samples + 1) * infos.samples_per_pixels * infos.rate_ratio
              );

        for(int k = 0; k < infos.nchannels; k++)
        {
          const int value = infos.half_h_int + int(mean_sample[k] * infos.half_h_ratio);
          p[k].drawLine(pPos[k], QPointF(x_pixels, value));
          pPos[k] = QPointF(x_pixels, value);
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Mean;
    result.zoom = ratio;
    result.x0 = infos.x0;
    result.xf = infos.x0 + infos.max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute_sample(const SizeInfos infos)
  {
    pen.setColor(b);
    QPainter* p = (QPainter*) alloca(sizeof(QPainter) * infos.nchannels);
    QVector<QImage> images;
    {
      QPainterCleanup _{p, infos.nchannels};

      if(!initImages(images, infos, p, _))
        return;

      int64_t oldbegin = 0, oldend = 0;
      for(int32_t x_samples = infos.x0, x_pixels = x_samples - infos.x0
          ;       x_samples < infos.xf && x_pixels < infos.max_pixel
          ;       x_samples++, x_pixels++
          )
      {
        if(computer.m_redraw_count > redraw_number)
          return;

        int64_t begin = (x_samples)     * infos.samples_per_pixels * infos.rate_ratio;
        int64_t end = (x_samples + 1)   * infos.samples_per_pixels * infos.rate_ratio;
        if(begin == oldend && end == begin)
          continue;
        oldbegin = begin;
        oldend = end;
        const auto mean_sample = data.frame(begin, end);

        for(int k = 0; k < infos.nchannels; k++)
        {
          QImage& image = images[k];
          auto dat = reinterpret_cast<uint32_t*>(image.bits());
          const int value = infos.half_h_int + int(mean_sample[k] * infos.half_h_ratio);

          auto [y, end_y] =
              value < infos.half_h_int
              ? std::tuple<int, int>{value, infos.half_h_int}
              : std::tuple<int, int>{infos.half_h_int, value};

          for(; y <= end_y; y++)
          {
            dat[x_pixels + y * infos.width] = r;
          }
          /*
          p[k].drawLine(QPointF(x_pixels, value), QPointF(x_pixels, infos.half_h));*/
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Sample;
    result.zoom = ratio;
    result.x0 = infos.x0;
    result.xf = infos.x0 + infos.max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute()
  {
    SizeInfos infos;
    LayerView& m_layer = computer.m_layer;
    QGraphicsView& m_view = computer.m_view;

    infos.nchannels = data.channels();
    if (infos.nchannels == 0)
      return;

    // Height of each channel
    infos.h = m_layer.height() / (float)infos.nchannels;
    if(infos.h < 1.)
      return;
    infos.w = m_layer.width();
    if(infos.w <= 1.)
      return;

    // leftmost point
    infos.x0 = std::max(std::floor(m_layer.mapFromScene(m_view.mapToScene(0, 0)).x()), 0.);

    auto& rms = data.rms();
    if (rms.frames_count == 0)
      return;

    // rightmost point
    const auto audioSampleRate = data.sampleRate();
    // There may be a ratio because the waveform could have been computed at a different samplerate.
    infos.rate_ratio = data.rms().sampleRateRatio(audioSampleRate);
    infos.samples_per_pixels = 0.001 * ratio * audioSampleRate;

    if(infos.samples_per_pixels <= 1e-6)
      return;

    LayerView& layer = computer.m_layer;
    QGraphicsView& view = computer.m_view;
    infos.xf = std::floor(layer.mapFromScene(view.mapToScene(view.width(), 0)).x());
    infos.rightmost_sample = (int32_t) (data.decodedSamples() / infos.samples_per_pixels);
    infos.xf = std::min(infos.xf, infos.rightmost_sample);

    infos.width = std::min(infos.w, (infos.xf - infos.x0));
    infos.max_pixel = std::min((int32_t)infos.width, (int32_t) infos.rightmost_sample);

    // height
    infos.h_int = infos.h;
    infos.h_ratio = -infos.h;
    infos.half_h = infos.h / 2.f;
    infos.half_h_int = infos.half_h;
    infos.half_h_ratio = 10 -infos.half_h;

    if(infos.width <= 1.)
      return;

    if(infos.samples_per_pixels <= 1.)
    {
      // Show lines in that case
      compute_sample(infos);
    }
    else if(infos.samples_per_pixels <= 16. * rms.header_ptr()->bufferSize)
    {
      // Show mean if one pixel is smaller than a rms sample
      compute_mean(infos);
    }
    else
    {
      // Show rms
      compute_rms(infos, rms);
    }
  }
};

void WaveformComputer::drawWaveFormsOnImage(
      const AudioFileHandle& data,
      ZoomRatio ratio,
      bool cols,
      int64_t redraw_number)
{
  WaveformComputerImpl impl{data, ratio, cols, redraw_number, *this};
  impl.compute();
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

