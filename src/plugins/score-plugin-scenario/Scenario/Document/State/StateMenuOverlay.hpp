#pragma once
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>

namespace Scenario
{
class CrossOverlay : public QGraphicsItem
{
public:
  CrossOverlay(StateView* parent)
    : QGraphicsItem{parent}
  {
    this->setAcceptHoverEvents(true);
  }

  static const constexpr int Type = ItemType::StateOverlay;
  int type() const final override
  { return Type; }

  QRectF boundingRect() const override
  { return {-1, -1, 16, 16}; }

  virtual const score::Brush& brush() const noexcept = 0;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto& pending_brush = brush();
    painter->setBrush(pending_brush);
    const auto bright = pending_brush.color();
    QPen p{bright.darker(300)};
    p.setWidth(2);
    painter->setPen(p);

    // TODO instead of a cross, make an arrow that looks like |->
    const auto small_rad = m_big ? 3. : 2.;

    const auto l1
        = m_big ? QLineF{QPointF{15 - small_rad, 0}, QPointF{15 - small_rad, 2 * small_rad}}
                      .translated(-3, 1)
                : QLineF{QPointF{15 - small_rad, 0}, QPointF{15 - small_rad, 2 * small_rad}}
                      .translated(-4, 2);
    const auto l2
        = m_big
              ? QLineF{QPointF{15 - 2 * small_rad, small_rad}, QPointF{15, small_rad}}.translated(
                  -3, 1)
              : QLineF{QPointF{15 - 2 * small_rad, small_rad}, QPointF{15, small_rad}}.translated(
                  -4, 2);

    painter->drawLine(l1.translated(1, 1));
    painter->drawLine(l2.translated(1, 1));
    p.setColor(bright);
    painter->setPen(p);
    painter->drawLine(l1);
    painter->drawLine(l2);

    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    auto p = event->pos();
    m_big = p.x() > 4 && p.y() < 10;
    update();
  }

  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
  {
    auto p = event->pos();
    m_big = p.x() > 4 && p.y() < 10;
    update();
  }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    m_big = false;
    update();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    auto st = static_cast<StateView*>(parentItem());
    st->presenter().moved(event->scenePos());
    event->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    auto st = static_cast<StateView*>(parentItem());
    st->presenter().released(event->scenePos());
    event->accept();
  }

  bool m_big{false};
};

class StatePlusOverlay final : public CrossOverlay
{
public:
  using CrossOverlay::CrossOverlay;

private:
  const score::Brush& brush() const noexcept override
  {
    auto& skin = Process::Style::instance();
    return skin.EventPending();
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    if (event->button() == Qt::MouseButton::LeftButton)
    {
      auto st = static_cast<StateView*>(parentItem());
      st->presenter().select();
      if (auto p = event->pos(); p.x() > 4 && p.y() < 10)
      {
        st->startCreateMode();
      }

      st->presenter().pressed(event->scenePos());
      event->accept();
    }
  }
};

class StateGraphPlusOverlay final : public CrossOverlay
{
public:
  using CrossOverlay::CrossOverlay;

private:
  const score::Brush& brush() const noexcept override
  {
    auto& skin = Process::Style::instance();
    return skin.EventDisposed();
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    if (event->button() == Qt::MouseButton::LeftButton)
    {
      auto st = static_cast<StateView*>(parentItem());
      st->presenter().select();
      if (auto p = event->pos(); p.x() > 4 && p.y() < 10)
      {
        st->startCreateGraphalMode();
      }

      st->presenter().pressed(event->scenePos());
      event->accept();
    }
  }
};

class StateSequencePlusOverlay final : public CrossOverlay
{
public:
  using CrossOverlay::CrossOverlay;

private:
  const score::Brush& brush() const noexcept override
  {
    auto& skin = Process::Style::instance();
    return skin.IntervalBase();
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    if (event->button() == Qt::MouseButton::LeftButton)
    {
      auto st = static_cast<StateView*>(parentItem());
      st->presenter().select();
      if (auto p = event->pos(); p.x() > 4 && p.y() < 10)
      {
        st->startCreateSequence();
      }

      st->presenter().pressed(event->scenePos());
      event->accept();
    }
  }
};

struct StateOverlays {
  StateOverlays(StateView* v)
  : m_overlay{v}
  , m_graphOverlay{v}
  , m_sequenceOverlay{v}
  {
    m_overlay.setPos(1, -14);
    m_graphOverlay.setPos(1, 6);
    if(v->presenter().model().nextInterval())
    {
      m_sequenceOverlay.setVisible(false);
      m_sequenceOverlay.setEnabled(false);
    }
    else
    {
      m_sequenceOverlay.setPos(9, -4);
    }
  }

  StatePlusOverlay m_overlay;
  StateGraphPlusOverlay m_graphOverlay;
  StateSequencePlusOverlay m_sequenceOverlay;
};
}
