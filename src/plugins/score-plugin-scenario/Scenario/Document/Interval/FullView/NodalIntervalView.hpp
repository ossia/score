#pragma once
#include <Process/Dataflow/NodeItem.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/graphics/RectItem.hpp>

#include <nano_observer.hpp>

namespace Scenario
{
class IntervalModel;
class NodalIntervalView final
    : public score::EmptyRectItem
    , public Nano::Observer
{
  W_OBJECT(NodalIntervalView)
public:
  enum ItemsToShow
  {
    AllItems,
    OnlyEffects
  };
  NodalIntervalView(
      ItemsToShow sh, const IntervalModel& model, const Process::Context& ctx,
      QGraphicsItem* parent);

  ~NodalIntervalView();

  void on_drop(QPointF pos, const QMimeData* data);
  void on_playPercentageChanged(double t, TimeVal parent_dur);

  void recenter();
  void recenterRelativeToView();
  void rescale();
  void zoomPlus();
  void zoomMinus();
  void zoomTo(double newZoomLevel);
  QRectF enclosingRect() const noexcept;

  QGraphicsItem& nodeContainer() const noexcept { return *m_container; }
  int type() const override { return ItemType::Type::NodalIntervalView; }

private:
  void on_processAdded(const Process::ProcessModel& proc);
  void on_processRemoving(const Process::ProcessModel& model);
  void on_zoomRatioChanged(ZoomRatio ratio);
  void on_dropOnNode(const QPointF& pt, const QMimeData& mime);

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void wheelEvent(QGraphicsSceneWheelEvent* event) override;

  const IntervalModel& m_model;
  const Process::Context& m_context;
  ItemsToShow m_itemsToShow{};
  std::vector<Process::NodeItem*> m_nodeItems;
  QGraphicsItem* m_container{};
  QPointF m_pressedPos{};
  double m_zoomLevel = 0;
};

}
