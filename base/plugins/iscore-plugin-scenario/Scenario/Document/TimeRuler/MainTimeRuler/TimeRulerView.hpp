#pragma once

#include <QRect>
#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>

namespace Scenario
{
class TimeRulerView final : public AbstractTimeRulerView
{
public:
  TimeRulerView();
  QRectF boundingRect() const override;
};
}
