#include "SoundView.hpp"
#include <Media/RMSData.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/hash.hpp>

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QDebug>
#include <QScrollBar>
#include <QTimer>
#include <cmath>

#include <score/tools/Debug.hpp>
#include <mutex>
#include <wobjectimpl.h>
#include <score/widgets/DoubleSlider.hpp>
#include <score/tools/std/Invoke.hpp>
#include <readerwriterqueue.h>

W_OBJECT_IMPL(Media::Sound::LayerView)
W_OBJECT_IMPL(Media::Sound::WaveformComputer)
namespace std
{
template<>
class hash<std::pair<int, int>>
{
public:
  std::size_t operator()(const std::pair<int, int>& p) const
  {
    std::size_t seed = 0;
    ossia::hash_combine(seed, p.first);
    ossia::hash_combine(seed, p.second);
    return seed;
  }
};
}
namespace Media
{
namespace Sound
{
struct QImagePool
{
  struct Images
  {
    QVector<QImage*> images;
    std::chrono::steady_clock::time_point last_touched;
  };

  static const constexpr int max_count = 100;
  using pool_t = ossia::fast_hash_map<std::pair<int, int>, Images>;
  pool_t pool;
  std::mutex m_mtx;

  static QImagePool& instance() noexcept
  {
    static QImagePool pool;
    return pool;
  }

  ~QImagePool()
  {
    for(auto& pair : pool)
    {
      for(QImage* img : pair.second.images)
      {
        delete img;
      }
    }
  }

  static inline int hit = 0;
  static inline int miss = 0;
  QImage* request(int w, int h)
  {
    std::lock_guard _{m_mtx};
    //cacheStats();
    auto it = pool.find({w, h});
    if(it != pool.end())
    {
      auto& vec = it->second.images;
      if(!vec.empty())
      {
        auto img = vec.front();
        vec.pop_front();
        img->fill(Qt::transparent);
        it->second.last_touched = std::chrono::steady_clock::now();
        hit++;
        return img;
      }
    }

    auto img = new QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    img->fill(Qt::transparent);
    miss++;
    return img;
  }

  void giveBack(const QVector<QImage*>& imgs)
  {
    for(auto img : imgs)
    {
      QVector<QImage*> to_delete;

      {
        std::lock_guard _{m_mtx};
        to_delete = gc();
        Images& images = pool[std::make_pair(img->width(), img->height())];
        SCORE_ASSERT(!images.images.contains(img));
        images.images.push_back(img);
        images.last_touched = std::chrono::steady_clock::now();
      }

      for(auto img : to_delete)
        delete img;
    }
  }

  QVector<QImage*> gc()
  {
    if(pool.empty())
      return {};

    int count = 0;
    auto oldest = pool.begin();
    auto oldest_t = oldest->second.last_touched;
    for(auto it = oldest; it != pool.end(); ++it)
    {
      if(it->second.last_touched < oldest->second.last_touched) {
        oldest = it;
        oldest_t = it->second.last_touched;
      }
      count += it->second.images.size();
    }

    if(count < max_count)
      return {};
    auto res = std::move(oldest->second.images);
    SCORE_ASSERT(oldest->second.images.isEmpty());
    oldest->second.last_touched = std::chrono::steady_clock::now();
    return res;
  }

  void cacheStats()
  {
    std::size_t bytes = 0;
    int images = 0;
    for(auto& pair : pool)
    {
      for(QImage* img : pair.second.images)
      {
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
        bytes += img->byteCount();
#else
        bytes += img->sizeInBytes();
#endif
        images ++;
      }
    }

    qDebug() << QString("%1 images: %2 megabytes ; hit/miss ratio : %3 / %4 = %5")
                .arg(images)
                .arg(bytes / (1024 * 1024))
                .arg(hit)
                .arg(miss)
                .arg(double(hit) / miss);
  }
};

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
      [=](QVector<QImage*> img, ComputedWaveform wf) {
        {
          QImagePool::instance().giveBack(m_images);
          m_images = std::move(img);

          // We display the image at the device ratio of the view
          auto view = ::getView(*this);
          if(view)
          {
            for(auto image : m_images)
            {
              image->setDevicePixelRatio(view->devicePixelRatioF());
            }
          }
        }
        m_wf = wf;

        update();
        //) << "ready!";
      });
}

LayerView::~LayerView()
{
  m_cpt->stop();
  m_cpt->moveToThread(this->thread());
  delete m_cpt;

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
    connect(
        &m_data->rms(),
        &RMSData::newData,
        this,
        &LayerView::on_newData,
        Qt::QueuedConnection);
    m_data->on_finishedDecoding.connect<&LayerView::on_finishedDecoding>(*this);
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
    m_recomputed = true;
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
  if (!m_data)
    return;

  int channels = m_images.size();
  if(channels == 0.)
  {
    if(!m_recomputed)
      m_cpt->recompute(m_data, m_zoom, m_frontColors);
    return;
  }

  auto ratio = m_wf.zoom / m_zoom;

  const qreal w = (m_wf.xf - m_wf.x0) * ratio;
  const qreal h = height() / channels;

  const double x0 = m_wf.x0 * ratio;

  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for(int i = 0; i < channels; i++)
  {
    painter->drawImage(QRectF{x0, h * i, w, h},* m_images[i]);
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
  //qDebug() << "finished decoding ! " ;
}

void LayerView::on_newData()
{
  recompute(m_zoom);
  //qDebug() << "new data ! " ;
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


WaveformComputer::WaveformComputer(LayerView& layer)
  : m_layer{layer}
  , m_view{*getView(m_layer)}
{
  connect(this, &WaveformComputer::recompute,
      this, [=] (const std::shared_ptr<AudioFile>& arg_1, double arg_2, bool cols) {
    int64_t n = ++m_redraw_count;

     // qDebug() << "count: " << n;
     ossia::qt::run_async(this, [=] { on_recompute(arg_1, arg_2, cols, n); });
  }, Qt::DirectConnection);
  startTimer(16, Qt::CoarseTimer);

  this->moveToThread(&m_drawThread);
  m_drawThread.start();
}

void WaveformComputer::stop()
{
  ossia::qt::run_async(this, [this] {
    moveToThread(m_layer.thread());
    m_drawThread.quit();
  });
  m_drawThread.wait();
}

struct WaveformComputerImpl
{
  AudioFile& data;
  AudioFile::ViewHandle& handle;
  ZoomRatio ratio;
  bool cols;
  int64_t redraw_number;
  WaveformComputer& computer;

  struct SizeInfos {
    int nchannels;
    float logical_samples_per_pixels;
    float physical_samples_per_pixels;
    static constexpr const double rate_ratio = 1.;


    float logical_h;
    int logical_h_int;
    float logical_h_ratio;
    float logical_half_h;
    int logical_half_h_int;
    float logical_half_h_ratio;

    int32_t logical_w;
    int32_t logical_x0;
    int32_t logical_xf;
    int32_t logical_width;
    int32_t logical_max_pixel;

    float physical_h;
    int physical_h_int;
    float physical_h_ratio;
    float physical_half_h;
    int physical_half_h_int;
    float physical_half_h_ratio;

    int32_t physical_w;
    int32_t physical_x0;
    int32_t physical_xf;
    int32_t physical_width;
    int32_t physical_max_pixel;


    int32_t decoded_samples;
    int32_t rightmost_sample;
  };

  static constexpr const auto orange = qRgba(250, 180, 15, 255);
  static constexpr const auto gray = qRgba(20, 81, 120, 255);
  const QPen orange_pen = [] { QPen p; p.setColor(orange);  p.setWidth(1); return p; }();
  const QPen gray_pen = [] { QPen p; p.setColor(gray);  p.setWidth(1); return p; }();

  const unsigned int main_color = cols ? orange : gray;
  const QPen& main_pen = cols ? orange_pen : gray_pen;

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


  bool initImages(QVector<QImage*>& images, const SizeInfos& infos) const noexcept
  {
    auto& pool = QImagePool::instance();
    images.resize(infos.nchannels);
    for(int i = 0; i < infos.nchannels; i++)
    {
      images[i] = pool.request(
            infos.physical_width,
            infos.physical_h);

      // No need to set device pixel ratio here, since we
      // change pixels directly
      if(computer.m_redraw_count > redraw_number)
        return false;
    }
    return true;
  }

  bool initImages(QVector<QImage*>& images, const SizeInfos& infos, QPainter* p, QPainterCleanup& _) const noexcept
  {
    auto& pool = QImagePool::instance();
    images.resize(infos.nchannels);
    for(int i = 0; i < infos.nchannels; i++)
    {
      images[i] = pool.request(
            infos.physical_width,
            infos.physical_h);

      // When painting on the image, we paint at retina resolution
      images[i]->setDevicePixelRatio(1.);

      if(computer.m_redraw_count > redraw_number)
        return false;

      p[i].begin(images[i]);
      p[i].setPen(this->main_pen);
      p[i].setRenderHint(QPainter::Antialiasing, true);
      _.init++;
    }
    return true;
  }
/*
  void compute_rms(const SizeInfos infos, const RMSData& rms)
  {
    QVector<QImage*> images;
    if(!initImages(images, infos))
      return;

    for(int32_t x_samples = infos.logical_x0, x_pixels = x_samples - infos.logical_x0
        ;       x_samples < infos.logical_xf && x_pixels < infos.logical_max_pixel
        ;       x_samples++, x_pixels++
        )
    {
      if(computer.m_redraw_count > redraw_number)
        return;

      const auto rms_sample = rms.frame(
            (x_samples)     * infos.logical_samples_per_pixels * infos.rate_ratio,
            (x_samples + 1) * infos.logical_samples_per_pixels * infos.rate_ratio
            );

      for(int k = 0; k < infos.nchannels; k++)
      {
        QImage& image = images[k];
        auto dat = reinterpret_cast<uint32_t*>(image.bits());

        const int value = infos.logical_half_h_int + int(rms_sample[k] * infos.logical_half_h_ratio);

        auto [y, end_y] =
            value < infos.logical_half_h_int
            ? std::tuple<int, int>{value, infos.logical_half_h_int}
            : std::tuple<int, int>{infos.logical_half_h_int, value};

        for(; y <= end_y; y++)
        {
          dat[x_pixels + y * infos.logical_width] = main_color;
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::RMS;
    result.zoom = ratio;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

    computer.ready(images, result);
  }
*/

  void compute_mean_absmax(const SizeInfos infos)
  {
    QPainter* p = (QPainter*) alloca(sizeof(QPainter) * infos.nchannels);
    QVector<QImage*> images;

    {
      QPainterCleanup _{p, infos.nchannels};

      ossia::small_vector<QPointF, 8> prev_pos(infos.nchannels);

      if(!initImages(images, infos, p, _))
        return;

      const float pix_ratio = infos.physical_samples_per_pixels * infos.rate_ratio;
      for(int32_t x_samples = infos.physical_x0, x_pixels = x_samples - infos.physical_x0
          ;     x_samples < infos.physical_xf && x_pixels < infos.physical_max_pixel
          ;     x_samples++, x_pixels++
          )
      {
        if(computer.m_redraw_count > redraw_number)
        {
          return;
        }

        int64_t start_sample =  x_samples * pix_ratio;
        int64_t end_sample =  (x_samples + 1) * pix_ratio;
        const auto mean_sample = handle.absmax_frame(start_sample, end_sample);

        for(int k = 0; k < infos.nchannels; k++)
        {
          const int max_value = infos.physical_half_h_int + int(mean_sample[k] * infos.physical_half_h_ratio);
          p[k].drawLine(prev_pos[k], QPointF(x_pixels, max_value));
          prev_pos[k] = QPointF(x_pixels, max_value);
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Mean;
    result.zoom = ratio;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute_mean_minmax(const SizeInfos infos)
  {
    //QPainter* p = (QPainter*) alloca(sizeof(QPainter) * infos.nchannels);
    QVector<QImage*> images;

    {
      //QPainterCleanup _{p, infos.nchannels};

      if(!initImages(images, infos/*, p, _*/))
        return;

      const float pix_ratio = infos.physical_samples_per_pixels * infos.rate_ratio;
      for(int32_t x_samples = infos.physical_x0, x_pixels = x_samples - infos.physical_x0
          ;     x_samples < infos.physical_xf && x_pixels < infos.physical_max_pixel
          ;     x_samples++, x_pixels++
          )
      {
        if(computer.m_redraw_count > redraw_number)
          return;

        int64_t start_sample =  x_samples * pix_ratio;
        if(start_sample >= infos.decoded_samples)
          break;

        int64_t end_sample =  (x_samples + 1) * pix_ratio;

        if(end_sample >= infos.decoded_samples)
          end_sample = infos.rightmost_sample;

        const auto mean_sample = handle.minmax_frame(start_sample, end_sample);

        for(int k = 0; k < infos.nchannels; k++)
        {
          const int min_value = ossia::clamp(infos.physical_half_h_int + int(mean_sample[k].first  * infos.physical_half_h_ratio), int(0), infos.physical_h_int - 1);
          const int max_value = ossia::clamp(infos.physical_half_h_int + int(mean_sample[k].second  * infos.physical_half_h_ratio), int(0), infos.physical_h_int - 1);

          QImage& image = *images[k];
          auto dat = reinterpret_cast<uint32_t*>(image.bits());
          for(int y = max_value; y <= min_value; y++)
          {
            dat[x_pixels + y * infos.physical_width] = main_color;
          }
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Mean;
    result.zoom = ratio;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute_sample(const SizeInfos infos)
  {
    QVector<QImage*> images;
    if(!initImages(images, infos))
      return;

    const float pix_ratio = infos.physical_samples_per_pixels * infos.rate_ratio;
    int64_t oldbegin = 0;
    for(int32_t x_samples = infos.physical_x0, x_pixels = x_samples - infos.physical_x0
        ;       x_samples < infos.physical_xf && x_pixels < infos.physical_max_pixel
        ;       x_samples++, x_pixels ++
        )
    {
      if(computer.m_redraw_count > redraw_number)
        return;

      int64_t begin = (x_samples)   * pix_ratio;
      if(begin == oldbegin)
        continue;
      oldbegin = begin;

      const auto frame = this->handle.frame(begin);

      SCORE_ASSERT(x_pixels >= 0 && x_pixels < infos.physical_width);
      for(int k = 0; k < infos.nchannels; k++)
      {
        QImage& image = *images[k];
        auto dat = reinterpret_cast<uint32_t*>(image.bits());
        const int v = infos.physical_half_h_int + int(frame[k] * infos.physical_half_h_ratio);
        const int value = ossia::clamp(v, int(0), int(infos.physical_h - 1));

        auto [y, end_y] =
            value < infos.physical_half_h_int
            ? std::tuple<int, int>{value, infos.physical_half_h_int}
          : std::tuple<int, int>{infos.physical_half_h_int, value};
        SCORE_ASSERT(y >= 0 && y < infos.physical_h);
        SCORE_ASSERT(end_y >= 0 && end_y <= infos.physical_h);

        for(; y <= end_y; y++)
        {
          dat[x_pixels + y * infos.physical_width] = main_color;
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Sample;
    result.zoom = ratio;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

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
    infos.logical_h = m_layer.height() / (float)infos.nchannels;
    if(infos.logical_h < 1.)
      return;
    infos.logical_w = m_layer.width();
    if(infos.logical_w <= 1.)
      return;

    // leftmost point
    infos.logical_x0 = std::max(std::floor(m_layer.mapFromScene(m_view.mapToScene(0, 0)).x()), 0.);

    auto& rms = data.rms();
    if (rms.frames_count == 0)
      return;

    // rightmost point
    const auto audioSampleRate = data.sampleRate();
    // not applicable if we source the direct MediaFileHnadle data which is always resampled,
    // only if we user the RMS cache :
    // There may be a ratio because the waveform could have been computed at a different samplerate.
    // infos.rate_ratio =  data.rms().sampleRateRatio(audioSampleRate);
    infos.logical_samples_per_pixels = 0.001 * ratio * audioSampleRate / ossia::flicks_per_millisecond<double>;
    if(infos.logical_samples_per_pixels <= 1e-6)
      return;

    LayerView& layer = computer.m_layer;
    QGraphicsView& view = computer.m_view;
    infos.logical_xf = std::floor(layer.mapFromScene(view.mapToScene(view.width(), 0)).x());
    infos.decoded_samples = data.decodedSamples();
    infos.rightmost_sample = (int32_t) (infos.decoded_samples / infos.logical_samples_per_pixels);
    infos.logical_xf = std::min(infos.logical_xf, infos.rightmost_sample);

    infos.logical_width = std::min(infos.logical_w, (infos.logical_xf - infos.logical_x0));
    infos.logical_max_pixel = std::min((int32_t)infos.logical_width, (int32_t) infos.rightmost_sample);

    // height
    infos.logical_h_int = infos.logical_h;
    infos.logical_h_ratio = -infos.logical_h;
    infos.logical_half_h = infos.logical_h / 2.f;
    infos.logical_half_h_int = infos.logical_half_h;
    infos.logical_half_h_ratio = 1 -infos.logical_half_h;

    if(infos.logical_width <= 1.)
      return;

    const auto dpr = m_view.devicePixelRatioF();
    infos.physical_samples_per_pixels = infos.logical_samples_per_pixels / dpr;

    infos.physical_h = dpr * infos.logical_h;
    infos.physical_h_int = dpr * infos.logical_h_int;
    infos.physical_h_ratio = dpr * infos.logical_h_ratio;
    infos.physical_half_h = dpr * infos.logical_half_h;
    infos.physical_half_h_int = dpr * infos.logical_half_h_int;
    infos.physical_half_h_ratio = dpr * infos.logical_half_h_ratio;

    infos.physical_w = dpr * infos.logical_w;
    infos.physical_x0 = dpr * infos.logical_x0;
    infos.physical_xf = dpr * infos.logical_xf;
    infos.physical_width = dpr * infos.logical_width;
    infos.physical_max_pixel = dpr * infos.logical_max_pixel;

    if(infos.physical_width * infos.physical_h > 3840 * 2160 * 3)
      return;
    if(infos.physical_width < 4 || infos.physical_h < 2)
      return;

    if(infos.logical_samples_per_pixels <= 1.)
    {
      // Show lines in that case
      compute_sample(infos);
    }
    else if(infos.logical_samples_per_pixels <= 10.)
    {
      // Show mean if one pixel is smaller than a rms sample
      compute_mean_absmax(infos);
    }
    else
    {
      // Show rms
      compute_mean_minmax(infos);
      //compute_rms(infos, rms);
    }
  }
};

void WaveformComputer::on_recompute(
    std::shared_ptr<AudioFile> data_qp,
    ZoomRatio ratio,
    bool cols,
    int64_t n)
{
  if(m_redraw_count > n)
    return;

  if (!data_qp)
    return;

  auto& data = *data_qp;

  if (data.channels() == 0)
    return;
  m_zoom = ratio;
  m_cols = cols;
  m_file = data_qp;
  m_n = n;
  last_request = std::chrono::steady_clock::now();
}

void WaveformComputer::timerEvent(QTimerEvent* event)
{
  if(!m_file)
    return;
  if(m_n == m_processed_n)
    return;

  // TODO if we haven't rendered for 24 ms maybe render the last thing ?
  using namespace std::literals;
  const auto now = std::chrono::steady_clock::now();
  if(now - last_request < 16ms && !(now - last_request > 32ms))
    return;

  auto dataHandle = m_file->handle();
  WaveformComputerImpl impl{*m_file, dataHandle, m_zoom, m_cols, m_n, *this};
  impl.compute();
  m_processed_n = m_n;
  //qDebug() << "finished processing" << m_processed_n;
}
}
}

