#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <QGraphicsItem>

class QGraphicsSceneMouseEvent;

namespace Scenario
{
class IntervalView;
class IntervalHeader : public QObject, public QGraphicsItem
{
public:
  enum class State
  {
    Hidden,     // No rack, we show nothing
    RackHidden, // There is at least a hidden rack in the interval
    RackShown   // There is a rack currently shown
  };

  using QGraphicsItem::QGraphicsItem;

  static const constexpr int Type = ItemType::IntervalHeader;
  int type() const final override { return Type; }

  void setIntervalView(IntervalView* view) { m_view = view; }
  static constexpr double headerHeight() { return IntervalHeaderHeight; }

  void setWidth(double width);
  virtual void setState(State s) = 0;
  State state() const noexcept { return m_state; }

  virtual void on_textChanged() {}

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

protected:
  IntervalView* m_view{};
  State m_state{};
  double m_width{};
};
}
