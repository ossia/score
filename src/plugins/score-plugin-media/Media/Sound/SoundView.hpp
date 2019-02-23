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
  QList<QPainterPath> arg_1;
  QPainterPath arg_2;
  double zoom;
  QImage img;
};

struct WaveformComputer : public QObject
{
  W_OBJECT(WaveformComputer)
public:
  enum action
  {
    KEEP_CUR = 0,
    USE_PREV,
    USE_NEXT,
    RECOMPUTE_ALL
  };
  W_ENUM(action, KEEP_CUR, USE_PREV, USE_NEXT, RECOMPUTE_ALL)

  LayerView& m_layer;
  WaveformComputer(LayerView& layer);
  std::atomic_bool dirty{};

  ~WaveformComputer() { m_drawThread.quit(); }
  void stop()
  {
    m_drawThread.quit();
    m_drawThread.wait();
  }

public:
  void recompute(std::shared_ptr<MediaFileHandle> arg_1, double arg_2)
      W_SIGNAL(recompute, arg_1, arg_2);
  void
  ready(QList<QPainterPath> arg_1, QPainterPath arg_2, double z, QImage img)
      W_SIGNAL(ready, arg_1, arg_2, z, img);

private:
  void on_recompute(std::shared_ptr<MediaFileHandle> data, double ratio);
  W_SLOT(on_recompute);

private:
  // Returns what to do depending on current density and stored density
  action compareDensity(const double density);

  // Computes a data set for the given ZoomRatio
  void computeDataSet(
      const MediaFileHandle& m_data,
      ZoomRatio ratio,
      double* densityptr,
      std::vector<ossia::float_vector>& dataset);

  void drawWaveForms(const MediaFileHandle& data, ZoomRatio ratio);
  ZoomRatio m_zoom{};

  double m_prevdensity = -1;
  double m_density = -1;
  double m_nextdensity = -1;

  std::vector<ossia::float_vector> m_prevdata;
  std::vector<ossia::float_vector> m_curdata;
  std::vector<ossia::float_vector> m_nextdata;

  QThread m_drawThread;
};

class LayerView final : public Process::LayerView
{
  W_OBJECT(LayerView)

public:
  explicit LayerView(QGraphicsItem* parent);
  ~LayerView();

  void setData(const std::shared_ptr<MediaFileHandle>& data);
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

  std::shared_ptr<MediaFileHandle> m_data;
  QList<QPainterPath> m_paths;
  QPainterPath m_channels{};
  int m_numChan{};
  int m_sampleRate{};

  ZoomRatio m_pathZoom{};
  ZoomRatio m_zoom{};
  void printAction(long);

  // QPixmap m_pixmap;
  WaveformComputer* m_cpt{};
};
}
}
