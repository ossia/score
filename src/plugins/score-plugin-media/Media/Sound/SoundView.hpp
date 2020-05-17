#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Process/LayerView.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/graphics/GraphicsItem.hpp>

#include <ossia/detail/pod_vector.hpp>

//#include <QFuture>
//#include <QFutureWatcher>
#include <verdigris>
namespace Media
{
namespace Sound
{
class LayerView;
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
class FilterWidget;
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
  ZoomRatio m_zoom{};

  std::atomic_int64_t m_redraw_count = std::numeric_limits<int64_t>::lowest();
  QThread m_drawThread;
  std::chrono::steady_clock::time_point last_request = std::chrono::steady_clock::now();

  std::shared_ptr<AudioFile> m_file{};
  bool m_cols{};
  int64_t m_n{};
  int64_t m_processed_n{-1};
  void timerEvent(QTimerEvent* event) override;
};

class LayerView final : public Process::LayerView, public Nano::Observer
{
  W_OBJECT(LayerView)

public:
  explicit LayerView(QGraphicsItem* parent);
  ~LayerView();

  void setData(const std::shared_ptr<AudioFile>& data);
  void setFrontColors(bool);
  void recompute(ZoomRatio ratio);

  void on_finishedDecoding();

private:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void heightChanged(qreal) override;
  void widthChanged(qreal) override;

  void scrollValueChanged(int);

  void on_newData();

  std::shared_ptr<AudioFile> m_data;
  int m_numChan{};
  int m_sampleRate{};

  ZoomRatio m_zoom{};
  void printAction(long);

  QVector<QImage*> m_images;
  // std::vector<QPixmap> m_pixmap;
  WaveformComputer* m_cpt{};

  ComputedWaveform m_wf{};

  bool m_frontColors{true};
  bool m_recomputed{false};

  friend class FilterWidget;
};
}
}

Q_DECLARE_METATYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(QVector<QImage>)
Q_DECLARE_METATYPE(QVector<QImage*>)
W_REGISTER_ARGTYPE(QVector<QImage*>)
