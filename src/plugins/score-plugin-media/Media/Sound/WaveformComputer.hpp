#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>

#include <score/tools/Debug.hpp>

#include <QImage>
#include <QObject>
#include <QVector>

#include <score_plugin_media_export.h>

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

  TimeVal startOffset{};
  TimeVal loopDuration{};
  bool loops{};

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

struct SCORE_PLUGIN_MEDIA_EXPORT WaveformComputer : public QObject
{
  W_OBJECT(WaveformComputer)
public:
  WaveformComputer();
  ~WaveformComputer();

  void stop();

public:
  void recompute(WaveformRequest req)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, recompute, req);

  void ready(QVector<QImage*> img, ComputedWaveform wf)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, ready, img, wf);

private:
  friend struct WaveformComputerImpl;

  void on_recompute(WaveformRequest&& req, int64_t n);
  void timerEvent(QTimerEvent* event) override;

  std::atomic_int64_t m_redraw_count = std::numeric_limits<int64_t>::lowest();
  std::chrono::steady_clock::time_point last_request = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last_render = {};
  bool m_forceRedraw = false;

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
