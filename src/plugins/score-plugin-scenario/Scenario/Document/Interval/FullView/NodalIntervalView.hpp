#pragma once
#include <Process/Dataflow/NodeItem.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/graphics/RectItem.hpp>

#include <QRectF>

class QGraphicsRectItem;

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

  //! Fits all the nodes in the view.
  void recenter();
  /**
   * @brief Places the canvas so that the model's nodal center is at the
   * center of the visible part of this item, at the model's scale.
   *
   * Called whenever the view's geometry changes; what is at the center stays
   * there. The first time, when the model has no center yet (new or older
   * document), the center of the nodes is taken and stored.
   */
  void recenterRelativeToView();
  //! Moves the canvas by `delta` (scene units), as dragging its background does.
  void panBy(QPointF delta);
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

  //! Center of the visible part of this item, in its coordinates.
  QPointF viewportCenter() const;
  //! Chooses and stores the model's nodal center (and scale) the first time
  //! the canvas is shown.
  void pickInitialViewport();
  //! Places the container so that `center` (container coordinates) is at viewportCenter().
  void placeContainer(QPointF center);
  //! Stores in the model what the container's current pos and scale show at viewportCenter().
  void storeCenterFromContainer();

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
  QGraphicsRectItem* m_selectionRect{};
  QPointF m_pressedPos{};
  double m_zoomLevel = 0;

  QPointF m_rubberBandOrigin{};
  QRectF m_rubberBandRect{};
  bool m_rubberBanding{false};
};

}
