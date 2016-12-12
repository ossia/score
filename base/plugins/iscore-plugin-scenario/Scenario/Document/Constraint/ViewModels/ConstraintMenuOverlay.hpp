#pragma once
#include <QGraphicsItem>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <QPainter>
#include <Process/Style/ScenarioStyle.hpp>
#include <QPen>
#include <QBrush>

namespace Scenario
{

struct ConstraintMenuOverlay final : public QGraphicsItem
{

public:
  ConstraintMenuOverlay(ConstraintView* parent):
    QGraphicsItem{parent}
  {

  }

  QRectF boundingRect() const override
  {
    return {0, 0, 20, 20};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = ScenarioStyle::instance();
    auto cst = static_cast<ConstraintView*>(parentItem());
    auto col = cst->constraintColor(skin);
    painter->setPen(col);
    painter->setBrush(col);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->drawChord(boundingRect(), 5760 / 2, -5760 / 2);

    painter->setBrush(skin.EventPending.getColor());
    QPen p{skin.EventPending.getColor()};
    p.setWidth(2);
    painter->setPen(p);

    painter->drawLine(QPointF{10, 2}, QPointF{10, 8});
    painter->drawLine(QPointF{7, 5}, QPointF{13, 5});
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
  }
};

}
