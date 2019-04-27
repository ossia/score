#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Process/LayerView.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/graphics/GraphicsItem.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <QPointer>
//#include <QFuture>
//#include <QFutureWatcher>
#include <wobjectdefs.h>
namespace Media
{
namespace Sound
{
class LayerView;
struct ComputedWaveform
{
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
  std::atomic_bool dirty{};

  ~WaveformComputer() { m_drawThread.quit(); }
  void stop()
  {
    m_drawThread.quit();
    m_drawThread.wait();
  }

public:
  void recompute(const std::shared_ptr<FFMPEGAudioFileHandle> &arg_1, double arg_2)
      W_SIGNAL(recompute, arg_1, arg_2);
  void
  ready(QVector<QImage> img, ComputedWaveform wf)
      W_SIGNAL(ready, img, wf);

private:
  void on_recompute(std::shared_ptr<FFMPEGAudioFileHandle> data, double ratio, int64_t n);
  W_SLOT(on_recompute);

private:
  void drawWaveFormsOnImage(
      const FFMPEGAudioFileHandle& data,
      ZoomRatio ratio,
      int64_t n);
  ZoomRatio m_zoom{};

  std::atomic_int64_t m_redraw_count = std::numeric_limits<int64_t>::lowest();
  QThread m_drawThread;
};

class LayerView final : public Process::LayerView
{
  W_OBJECT(LayerView)

public:
  explicit LayerView(QGraphicsItem* parent);
  ~LayerView();

  void setData(const std::shared_ptr<FFMPEGAudioFileHandle>& data);
  void recompute(ZoomRatio ratio);

private:
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void scrollValueChanged(int);

  void on_finishedDecoding();
  void on_newData();

  std::shared_ptr<FFMPEGAudioFileHandle> m_data;
  int m_numChan{};
  int m_sampleRate{};

  ZoomRatio m_zoom{};
  void printAction(long);

  std::vector<QPixmap> m_pixmap;
  QVector<QImage> m_image;
  WaveformComputer* m_cpt{};

  ComputedWaveform m_wf{};
};
}
}

Q_DECLARE_METATYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(Media::Sound::ComputedWaveform)
W_REGISTER_ARGTYPE(QVector<QImage>)
