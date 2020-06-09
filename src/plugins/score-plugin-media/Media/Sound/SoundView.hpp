#pragma once
#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/WaveformComputer.hpp>
#include <Process/LayerView.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/graphics/GraphicsItem.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <verdigris>
namespace Media
{
namespace Sound
{
class LayerView final : public Process::LayerView, public Nano::Observer
{
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
  WaveformComputer* m_cpt{};

  ComputedWaveform m_wf{};

  bool m_frontColors{true};
  bool m_recomputed{false};

  friend class FilterWidget;
};
}
}
