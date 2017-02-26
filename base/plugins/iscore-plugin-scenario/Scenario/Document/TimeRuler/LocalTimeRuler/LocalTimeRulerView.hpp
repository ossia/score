#pragma once

#include <QRect>
#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>

namespace Scenario
{
class LocalTimeRulerView final : public AbstractTimeRulerView
{
public:
  LocalTimeRulerView(QGraphicsView*);
  ~LocalTimeRulerView();

  QRectF boundingRect() const override;
};
}
