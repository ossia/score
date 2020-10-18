#pragma once
#include <Process/Dataflow/NodeItem.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <nano_observer.hpp>

namespace Scenario
{
class NodalIntervalView : public score::EmptyRectItem, public Nano::Observer
{
public:
  enum ItemsToShow {
    AllItems,
    OnlyEffects
  };
  NodalIntervalView(ItemsToShow sh, const IntervalModel& model, const Process::Context& ctx, QGraphicsItem* parent);

  ~NodalIntervalView();

  void on_drop(QPointF pos, const QMimeData* data);
  void on_playPercentageChanged(double t);

  QRectF enclosingRect() const noexcept;

  /*
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget
  *widget) override
  {
    painter->fillRect(m_rect, Qt::blue);
  }
  */

private:
  void on_processAdded(const Process::ProcessModel& proc);
  void on_processRemoving(const Process::ProcessModel& model);
  void on_zoomRatioChanged(ZoomRatio ratio);

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  const IntervalModel& m_model;
  const Process::Context& m_context;
  ItemsToShow m_itemsToShow{};
  std::vector<Process::NodeItem*> m_nodeItems;

};

}
