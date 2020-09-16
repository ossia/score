#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>
#include <QObject>
#include <QVector>
#include <QImage>
#include <QThread>

#include <memory>
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
  bool colors{};
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

struct WaveformComputer : public QObject
{
  W_OBJECT(WaveformComputer)
public:
  WaveformComputer();
  ~WaveformComputer();

  void stop();

public:
  void recompute(WaveformRequest req)
  W_SIGNAL(recompute, req);

  void ready(QVector<QImage*> img, ComputedWaveform wf)
  W_SIGNAL(ready, img, wf);

private:
  friend struct WaveformComputerImpl;

  void on_recompute(WaveformRequest&& req, int64_t n);
  void timerEvent(QTimerEvent* event) override;


  std::atomic_int64_t m_redraw_count = std::numeric_limits<int64_t>::lowest();
  std::chrono::steady_clock::time_point last_request = std::chrono::steady_clock::now();

  WaveformRequest m_currentRequest;

  int64_t m_n{};
  int64_t m_processed_n{-1};
};

struct WaveformThreads
{
  WaveformThreads();

  static WaveformThreads& instance();

  QThread* acquireThread();
  void releaseThread();

  std::unique_ptr<QThread[]> threads;
  int numThreads{};
  int currentThread{};

  int inFlight = 0;
};

}

Q_DECLARE_METATYPE(Media::Sound::WaveformRequest)
W_REGISTER_ARGTYPE(Media::Sound::WaveformRequest)
Q_DECLARE_METATYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(QVector<QImage>)
Q_DECLARE_METATYPE(QVector<QImage*>)
W_REGISTER_ARGTYPE(QVector<QImage*>)
