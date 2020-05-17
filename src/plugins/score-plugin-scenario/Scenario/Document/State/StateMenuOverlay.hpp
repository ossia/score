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

class StatePlusOverlay final : public QGraphicsItem
{
public:
  StatePlusOverlay(StateView* parent) : QGraphicsItem{parent} { this->setAcceptHoverEvents(true); }

  static const constexpr int Type = ItemType::StateOverlay;
  int type() const final override { return Type; }

  QRectF boundingRect() const override { return {-1, -1, 16, 16}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = Process::Style::instance();

    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto& pending_brush = skin.EventPending();
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

private:
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

  bool m_big{false};
};

class StateGraphPlusOverlay final : public QGraphicsItem
{
public:
  StateGraphPlusOverlay(StateView* parent) : QGraphicsItem{parent}
  {
    this->setAcceptHoverEvents(true);
  }

  static const constexpr int Type = ItemType::StateOverlay; // FIXME should be different
  int type() const final override { return Type; }

  QRectF boundingRect() const override { return {-1, -1, 16, 16}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = Process::Style::instance();

    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto& pending_brush = skin.EventDisposed();
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

private:
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

  bool m_big{false};
};
}
