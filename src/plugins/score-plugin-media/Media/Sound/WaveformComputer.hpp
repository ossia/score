#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>
#include <QObject>
#include <QVector>
#include <QImage>
#include <verdigris>

class QGraphicsView;
namespace Media::Sound
{
class LayerView;
class FilterWidget;
struct WaveformComputerImpl;

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
  WaveformComputer(LayerView& layer);

  LayerView& m_layer;
  QGraphicsView& m_view;

  ~WaveformComputer() { m_drawThread.quit(); }
  void stop();

public:
  void recompute(const std::shared_ptr<AudioFile>& arg_1, double arg_2, bool cols)
      W_SIGNAL(recompute, arg_1, arg_2, cols);
  void ready(QVector<QImage*> img, ComputedWaveform wf) W_SIGNAL(ready, img, wf);

private:
  void on_recompute(std::shared_ptr<AudioFile> data, double ratio, bool cols, int64_t n);
  W_SLOT(on_recompute);

private:
  friend struct WaveformComputerImpl;
  void timerEvent(QTimerEvent* event) override;

  ZoomRatio m_zoom{};

  std::atomic_int64_t m_redraw_count = std::numeric_limits<int64_t>::lowest();
  QThread m_drawThread;
  std::chrono::steady_clock::time_point last_request = std::chrono::steady_clock::now();

  std::shared_ptr<AudioFile> m_file{};
  bool m_cols{};
  int64_t m_n{};
  int64_t m_processed_n{-1};
};

}

Q_DECLARE_METATYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(QVector<QImage>)
Q_DECLARE_METATYPE(QVector<QImage*>)
W_REGISTER_ARGTYPE(QVector<QImage*>)
