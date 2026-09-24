#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>

#include <score/tools/Debug.hpp>

#include <QImage>
#include <QObject>
#include <QVector>

#include <score_plugin_media_export.h>

#include <memory>
#include <mutex>
#include <verdigris>

class QGraphicsView;
namespace Media::Sound
{
class LayerView;
struct WaveformComputerImpl;

struct WaveformRequest
{
  std::shared_ptr<AudioFile> file;
  double zoom{};
  double tempo_ratio{};

  QSizeF layerSize{};
  double devicePixelRatio{};
  double view_x0{};
  double view_xmax{};

  TimeVal startOffset{};
  TimeVal loopDuration{};
  bool loops{};

  bool colors{};

  //! Two requests that compare equal produce the same image, so the second one
  //! is not worth rendering. Everything the computer reads is in here except
  //! the skin, which the view invalidates by hand when it changes.
  friend bool operator==(const WaveformRequest&, const WaveformRequest&) = default;
};

struct ComputedWaveform
{
  enum Mode
  {
    RMS,
    Mean,
    Sample
  } mode{};
  double zoom{};

  int x0{};
  int xf{};
};

//! Largest image the computer will rasterise, in physical pixels. A request
//! above it produces nothing at all, so callers must not make one.
static constexpr int64_t maxWaveformPixels = 3840ll * 2160 * 3;

struct SCORE_PLUGIN_MEDIA_EXPORT WaveformComputer : public QObject
{
  W_OBJECT(WaveformComputer)
public:
  WaveformComputer(bool threaded = true);
  ~WaveformComputer();

  void stop();

  //! Takes ownership of images received from ready(). Every receiver of
  //! ready() must call this before keeping them: until then the computer owns
  //! them, and returns unclaimed ones (event dropped with its receiver) to
  //! the pool when it dies.
  void claim(const QVector<QImage*>& img);

public:
  void recompute(WaveformRequest req)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, recompute, req);

  void ready(QVector<QImage*> img, ComputedWaveform wf)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, ready, img, wf);

private:
  friend struct WaveformComputerImpl;

  void on_recompute(WaveformRequest&& req, int64_t n);
  void timerEvent(QTimerEvent* event) override;
  //! Emits ready(), keeping the images in flight until claim()ed.
  void deliver(QVector<QImage*> img, ComputedWaveform wf);

  std::mutex m_inflightMutex;
  QVector<QImage*> m_inflight;

  std::atomic_int64_t m_redraw_count = std::numeric_limits<int64_t>::lowest();
  std::chrono::steady_clock::time_point last_render = {};

  //! How long the previous render took; the rate limit is derived from it.
  std::chrono::steady_clock::duration m_lastRenderDuration{};

  WaveformRequest m_currentRequest;

  int64_t m_n{};
  int64_t m_processed_n{-1};

  std::shared_ptr<AudioFile> m_currentFile;
  Media::AudioFile::ViewHandle m_currentView;
  std::atomic_bool m_abort{};
};

}

inline QDataStream& operator<<(QDataStream& i, const Media::Sound::WaveformRequest& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, Media::Sound::WaveformRequest& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator<<(QDataStream& i, const Media::Sound::ComputedWaveform& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, Media::Sound::ComputedWaveform& sel)
{
  SCORE_ABORT;
  return i;
}
Q_DECLARE_METATYPE(Media::Sound::WaveformRequest)
W_REGISTER_ARGTYPE(Media::Sound::WaveformRequest)
Q_DECLARE_METATYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(QVector<QImage>)
Q_DECLARE_METATYPE(QVector<QImage*>)
W_REGISTER_ARGTYPE(QVector<QImage*>)
