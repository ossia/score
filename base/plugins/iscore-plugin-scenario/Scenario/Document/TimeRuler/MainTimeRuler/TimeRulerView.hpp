#pragma once

#include <QRect>
#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
class QGraphicsView;
namespace Scenario
{
class TimeRulerView final : public AbstractTimeRulerView
{
public:
  TimeRulerView(QGraphicsView*);
  QRectF boundingRect() const override;
};
}
