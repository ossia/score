#include "WaveformComputer.hpp"
#include <Media/Sound/SoundView.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/RMSData.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/std/Invoke.hpp>
#include <score/tools/ThreadPool.hpp>

#include <ossia/detail/math.hpp>

#include <QColor>
#include <QPainter>
#include <QGraphicsView>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Media::Sound::WaveformComputer)
namespace Media::Sound
{
WaveformComputer::WaveformComputer()
{
  connect(
      this,
      &WaveformComputer::recompute,
      this,
      [=](WaveformRequest req) {
        int64_t n = ++m_redraw_count;

        ossia::qt::run_async(this, [=, r = std::move(req)] () mutable {
          on_recompute(std::move(r), n);
        });
      },
      Qt::DirectConnection);
  startTimer(16, Qt::CoarseTimer);

  auto& inst = score::ThreadPool::instance();
  this->moveToThread(inst.acquireThread());
}

WaveformComputer::~WaveformComputer()
{
}

void WaveformComputer::stop()
{
}

struct WaveformComputerImpl
{
  struct LoopWrapper {
    AudioFile::ViewHandle& handle;

    int64_t decoded_samples{};
    int64_t start_offset{};
    int64_t duration{};

    using frame_fun_t = bool (*)(LoopWrapper& h, int64_t start_frame, ossia::small_vector<float, 8>& out) noexcept;
    using absmax_frame_fun_t = bool (*)(LoopWrapper& h, int64_t start_frame, int64_t end_frame, ossia::small_vector<float, 8>& out) noexcept;
    using minmax_frame_fun_t = bool (*)(LoopWrapper& h, int64_t start_frame, int64_t end_frame, ossia::small_vector<std::pair<float, float>, 8>& out) noexcept;

    frame_fun_t frame_impl{};
    absmax_frame_fun_t absmax_frame_impl{};
    minmax_frame_fun_t minmax_frame_impl{};

    // TODO could be worth memoizing in a thread_local vector for absmax / minmax if we have a lot
    // of loops


    bool frame(int64_t start_frame, ossia::small_vector<float, 8>& out)
    {
      return frame_impl(*this, start_frame, out);
    }
    bool absmax_frame(int64_t start_frame, int64_t end_frame, ossia::small_vector<float, 8>& out)
    {
      return absmax_frame_impl(*this, start_frame, end_frame, out);
    }
    bool minmax_frame(int64_t start_frame, int64_t end_frame, ossia::small_vector<std::pair<float, float>, 8>& out)
    {
      return minmax_frame_impl(*this, start_frame, end_frame, out);
    }

    static bool normal_frame(LoopWrapper& h, int64_t start_frame, ossia::small_vector<float, 8>& out) noexcept
    {
      const int64_t start = h.start_offset + start_frame;
      if(start < h.decoded_samples)
      {
         h.handle.frame(start, out);
         return true;
      }
      else
      {
        return false;
      }
    }
    static bool normal_absmax_frame(LoopWrapper& h, int64_t start_frame, int64_t end_frame, ossia::small_vector<float, 8>& out) noexcept
    {
      const int64_t start = h.start_offset + start_frame;
      const int64_t end = h.start_offset + end_frame;
      if(start < h.decoded_samples && end < h.decoded_samples)
      {
        h.handle.absmax_frame(start, end, out);
        return true;
      }
      else
      {
        return false;
      }
    }
    static bool normal_minmax_frame(LoopWrapper& h, int64_t start_frame, int64_t end_frame, ossia::small_vector<std::pair<float, float>, 8>& out) noexcept
    {
      const int64_t start = h.start_offset + start_frame;
      const int64_t end = h.start_offset + end_frame;
      if(start < h.decoded_samples && end < h.decoded_samples)
      {
        h.handle.minmax_frame(start, end, out);
        return true;
      }
      else
      {
        return false;
      }
    }

    static bool loop_frame(LoopWrapper& h, int64_t start_frame, ossia::small_vector<float, 8>& out) noexcept
    {
      const int64_t start = h.start_offset + start_frame % h.duration;
      if(start < h.decoded_samples)
      {
        h.handle.frame(start % h.duration, out);
        return true;
      }
      else
      {
        return false;
      }
    }
    static bool loop_absmax_frame(LoopWrapper& h, int64_t start_frame, int64_t end_frame, ossia::small_vector<float, 8>& out) noexcept
    {
      const int64_t start = h.start_offset + start_frame % h.duration;
      const int64_t end = h.start_offset + end_frame % h.duration;
      if(start < end)
        h.handle.absmax_frame(start, end, out);
      else
        h.handle.absmax_frame(start, start, out); // TODO can maybe be improved
      return true;
    }
    static bool loop_minmax_frame(LoopWrapper& h, int64_t start_frame, int64_t end_frame, ossia::small_vector<std::pair<float, float>, 8>& out) noexcept
    {
      const int64_t start = h.start_offset + start_frame % h.duration;
      const int64_t end = h.start_offset + end_frame % h.duration;
      if(start < end)
        h.handle.minmax_frame(start, end, out);
      else
        h.handle.minmax_frame(start, start, out); // TODO can maybe be improved
      return true;
    }
  } handle;

  const WaveformRequest& request;
  int64_t redraw_number;
  WaveformComputer& computer;
  QImagePool& pool = QImagePool::instance();

  struct SizeInfos
  {
    int nchannels;
    float logical_samples_per_pixels;
    float physical_samples_per_pixels;
    float tempo_ratio;
    float pixel_ratio;

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

    int32_t rightmost_sample;
  };

  static constexpr const auto orange = qRgba(250, 180, 15, 255);
  static constexpr const auto gray = qRgba(20, 81, 120, 255);
  const QPen orange_pen = [] {
    QPen p;
    p.setColor(orange);
    p.setWidth(1);
    return p;
  }();
  const QPen gray_pen = [] {
    QPen p;
    p.setColor(gray);
    p.setWidth(1);
    return p;
  }();

  const unsigned int main_color = request.colors ? orange : gray;
  const QPen& main_pen = request.colors ? orange_pen : gray_pen;

  struct QPainterCleanup
  {
    QPainter* p;
    int n;
    int init = 0;
    QPainterCleanup(QPainter* p, int n) : p{p}, n{n}
    {
      for (int i = 0; i < n; i++)
      {
        new (&p[i]) QPainter;
      }
    }
    ~QPainterCleanup()
    {
      for (int i = 0; i < init; i++)
      {
        p[i].end();
      }

      for (int i = 0; i < n; i++)
      {
        p[i].~QPainter();
      }
    }
  };

  bool initImages(QVector<QImage*>& images, const SizeInfos& infos) const noexcept
  {
    images.resize(infos.nchannels);
    for (int i = 0; i < infos.nchannels; i++)
    {
      images[i] = pool.request(infos.physical_width, infos.physical_h);

      // No need to set device pixel ratio here, since we
      // change pixels directly
      if (computer.m_redraw_count > redraw_number)
      {
        images.resize(i + 1);
        pool.giveBack(images);
        return false;
      }
    }
    return true;
  }

  bool
  initImages(QVector<QImage*>& images, const SizeInfos& infos, QPainter* p, QPainterCleanup& _)
      const noexcept
  {
    images.resize(infos.nchannels);
    for (int i = 0; i < infos.nchannels; i++)
    {
      images[i] = pool.request(infos.physical_width, infos.physical_h);

      // When painting on the image, we paint at retina resolution
      images[i]->setDevicePixelRatio(1.);

      if (computer.m_redraw_count > redraw_number)
      {
        images.resize(i + 1);
        pool.giveBack(images);
        return false;
      }

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

      for(int32_t x_samples = infos.logical_x0, x_pixels = x_samples -
    infos.logical_x0 ;       x_samples < infos.logical_xf && x_pixels <
    infos.logical_max_pixel ;       x_samples++, x_pixels++
          )
      {
        if(computer.m_redraw_count > redraw_number)
        {
          pool.giveBack(images);
          return;
        }

        const auto rms_sample = rms.frame(
              (x_samples)     * infos.logical_samples_per_pixels *
    infos.rate_ratio, (x_samples + 1) * infos.logical_samples_per_pixels *
    infos.rate_ratio
              );

        for(int k = 0; k < infos.nchannels; k++)
        {
          QImage& image = images[k];
          auto dat = reinterpret_cast<uint32_t*>(image.bits());

          const int value = infos.logical_half_h_int + int(rms_sample[k] *
    infos.logical_half_h_ratio);

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
    QPainter* p = (QPainter*)alloca(sizeof(QPainter) * infos.nchannels);
    QVector<QImage*> images;

    {
      QPainterCleanup _{p, infos.nchannels};

      ossia::small_vector<QPointF, 8> prev_pos(infos.nchannels);

      if (!initImages(images, infos, p, _))
        return;

      ossia::small_vector<float, 8> mean_sample;
      const float pix_ratio = infos.pixel_ratio;
      for (int32_t x_samples = infos.physical_x0, x_pixels = x_samples - infos.physical_x0;
           x_samples < infos.physical_xf && x_pixels < infos.physical_max_pixel;
           x_samples++, x_pixels++)
      {
        if (computer.m_redraw_count > redraw_number)
        {
          pool.giveBack(images);
          return;
        }

        int64_t start_sample = x_samples * pix_ratio;
        int64_t end_sample = (x_samples + 1) * pix_ratio;

        bool ok = handle.absmax_frame(start_sample, end_sample, mean_sample);
        if(!ok)
          break;

        for (int k = 0; k < infos.nchannels; k++)
        {
          const int max_value
              = infos.physical_half_h_int + int(mean_sample[k] * infos.physical_half_h_ratio);
          p[k].drawLine(prev_pos[k], QPointF(x_pixels, max_value));
          prev_pos[k] = QPointF(x_pixels, max_value);
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Mean;
    result.zoom = request.zoom;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute_mean_minmax(const SizeInfos infos)
  {
    // QPainter* p = (QPainter*) alloca(sizeof(QPainter) * infos.nchannels);
    QVector<QImage*> images;

    {
      // QPainterCleanup _{p, infos.nchannels};

      if (!initImages(images, infos /*, p, _*/))
        return;

      ossia::small_vector<std::pair<float, float>, 8> mean_sample;
      const float pix_ratio = infos.pixel_ratio;
      for (int32_t x_samples = infos.physical_x0, x_pixels = x_samples - infos.physical_x0;
           x_samples < infos.physical_xf && x_pixels < infos.physical_max_pixel;
           x_samples++, x_pixels++)
      {
        if (computer.m_redraw_count > redraw_number)
        {
          pool.giveBack(images);
          return;
        }

        int64_t start_sample = x_samples * pix_ratio;
        int64_t end_sample = (x_samples + 1) * pix_ratio;

        bool ok = handle.minmax_frame(start_sample, end_sample, mean_sample);
        if(!ok)
          break;

        for (int k = 0; k < infos.nchannels; k++)
        {
          const int min_value = ossia::clamp(
              infos.physical_half_h_int + int(mean_sample[k].first * infos.physical_half_h_ratio),
              int(0),
              infos.physical_h_int - 1);
          const int max_value = ossia::clamp(
              infos.physical_half_h_int + int(mean_sample[k].second * infos.physical_half_h_ratio),
              int(0),
              infos.physical_h_int - 1);

          QImage& image = *images[k];
          auto dat = reinterpret_cast<uint32_t*>(image.bits());
          for (int y = max_value; y <= min_value; y++)
          {
            dat[x_pixels + y * infos.physical_width] = main_color;
          }
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Mean;
    result.zoom = request.zoom;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute_sample(const SizeInfos infos)
  {
    QVector<QImage*> images;
    if (!initImages(images, infos))
      return;

    ossia::small_vector<float, 8> frame;
    const float pix_ratio = infos.pixel_ratio;
    int64_t oldbegin = 0;
    for (int32_t x_samples = infos.physical_x0, x_pixels = x_samples - infos.physical_x0;
         x_samples < infos.physical_xf && x_pixels < infos.physical_max_pixel;
         x_samples++, x_pixels++)
    {
      if (computer.m_redraw_count > redraw_number)
      {
        pool.giveBack(images);
        return;
      }

      int64_t begin = (x_samples)*pix_ratio;
      if (begin == oldbegin)
        continue;
      oldbegin = begin;

      bool ok = this->handle.frame(begin, frame);
      if(!ok)
        break;

      SCORE_ASSERT(x_pixels >= 0 && x_pixels < infos.physical_width);
      for (int k = 0; k < infos.nchannels; k++)
      {
        QImage& image = *images[k];
        auto dat = reinterpret_cast<uint32_t*>(image.bits());
        const int v = infos.physical_half_h_int + int(frame[k] * infos.physical_half_h_ratio);
        const int value = ossia::clamp(v, int(0), int(infos.physical_h - 1));

        auto [y, end_y] = value < infos.physical_half_h_int
                              ? std::tuple<int, int>{value, infos.physical_half_h_int}
                              : std::tuple<int, int>{infos.physical_half_h_int, value};
        SCORE_ASSERT(y >= 0 && y < infos.physical_h);
        SCORE_ASSERT(end_y >= 0 && end_y <= infos.physical_h);

        for (; y <= end_y; y++)
        {
          dat[x_pixels + y * infos.physical_width] = main_color;
        }
      }
    }

    ComputedWaveform result;
    result.mode = ComputedWaveform::Sample;
    result.zoom = request.zoom;
    result.x0 = infos.logical_x0;
    result.xf = infos.logical_x0 + infos.logical_max_pixel;

    computer.ready(std::move(images), result);
  }

  void compute()
  {
    SizeInfos infos;
    const auto& data = *request.file;
    const auto zoom = request.zoom;

    infos.nchannels = data.channels();
    if (infos.nchannels == 0)
      return;

    // Height of each channel
    infos.logical_h = request.layerSize.height() / (float)infos.nchannels;
    if (infos.logical_h < 1.)
      return;
    infos.logical_w = request.layerSize.width();
    if (infos.logical_w <= 1.)
      return;

    // leftmost point
    infos.logical_x0 = std::max(std::floor(request.view_x0), 0.);
    infos.tempo_ratio = request.tempo_ratio;

    auto& rms = data.rms();
    if (rms.frames_count == 0)
      return;

    // rightmost point
    const auto audioSampleRate = data.sampleRate();
    // not applicable if we source the direct MediaFileHnadle data which is
    // always resampled, only if we user the RMS cache : There may be a ratio
    // because the waveform could have been computed at a different samplerate.
    // infos.rate_ratio =  data.rms().sampleRateRatio(audioSampleRate);
    infos.logical_samples_per_pixels
        = infos.tempo_ratio * 0.001 * zoom * audioSampleRate / (ossia::flicks_per_millisecond<double>);
    if (infos.logical_samples_per_pixels <= 1e-6)
      return;

    infos.logical_xf = std::floor(request.view_xmax);
    infos.rightmost_sample = (int32_t)(handle.decoded_samples / infos.logical_samples_per_pixels);
    infos.logical_xf = infos.logical_xf;

    infos.logical_width = std::min(infos.logical_w, (infos.logical_xf - infos.logical_x0));
    infos.logical_max_pixel = infos.logical_width;
//        = std::min((int32_t)infos.logical_width, (int32_t)infos.rightmost_sample);

    // height
    infos.logical_h_int = infos.logical_h;
    infos.logical_h_ratio = -infos.logical_h;
    infos.logical_half_h = infos.logical_h / 2.f;
    infos.logical_half_h_int = infos.logical_half_h;
    infos.logical_half_h_ratio = 1 - infos.logical_half_h;

    if (infos.logical_width <= 1.)
      return;

    const auto dpr = request.devicePixelRatio;
    infos.physical_samples_per_pixels = infos.logical_samples_per_pixels / dpr;
    infos.pixel_ratio = infos.physical_samples_per_pixels;

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

    if (infos.physical_width * infos.physical_h > 3840 * 2160 * 3)
      return;
    if (infos.physical_width < 4 || infos.physical_h < 2)
      return;

    if (infos.logical_samples_per_pixels <= 1.)
    {
      // Show lines in that case
      compute_sample(infos);
    }
    else if (infos.logical_samples_per_pixels <= 10.)
    {
      // Show mean if one pixel is smaller than a rms sample
      compute_mean_absmax(infos);
    }
    else
    {
      // Show rms
      compute_mean_minmax(infos);
      // compute_rms(infos, rms);
    }
  }
};

void WaveformComputer::on_recompute(
    WaveformRequest&& req,
    int64_t n)
{
  if (m_redraw_count > n)
    return;

  if (!req.file)
    return;

  if (req.file->channels() == 0)
    return;

  m_currentRequest = std::move(req);
  m_n = n;

  last_request = std::chrono::steady_clock::now();
}

void WaveformComputer::timerEvent(QTimerEvent* event)
{
  auto& file = m_currentRequest.file;
  if (!file)
    return;
  if (m_n == m_processed_n)
    return;

  // TODO if we haven't rendered for 24 ms maybe render the last thing ?
  using namespace std::literals;
  const auto now = std::chrono::steady_clock::now();
  if (now - last_request < 16ms && !(now - last_request > 32ms))
    return;

  auto dataHandle = file->handle();
  const double rate = file->sampleRate();
  WaveformComputerImpl::LoopWrapper loopHandle{
    dataHandle,
    file->decodedSamples(),
    m_currentRequest.startOffset.toSample(rate * m_currentRequest.tempo_ratio),
    m_currentRequest.loopDuration.toSample(rate * m_currentRequest.tempo_ratio)
  };
  if(m_currentRequest.loops)
  {
    loopHandle.frame_impl = loopHandle.loop_frame;
    loopHandle.absmax_frame_impl = loopHandle.loop_absmax_frame;
    loopHandle.minmax_frame_impl = loopHandle.loop_minmax_frame;
  }
  else
  {
    loopHandle.frame_impl = loopHandle.normal_frame;
    loopHandle.absmax_frame_impl = loopHandle.normal_absmax_frame;
    loopHandle.minmax_frame_impl = loopHandle.normal_minmax_frame;
  }
  WaveformComputerImpl impl{loopHandle, m_currentRequest, m_n, *this};
  impl.compute();
  m_processed_n = m_n;
  // qDebug() << "finished processing" << m_processed_n;
}





}
