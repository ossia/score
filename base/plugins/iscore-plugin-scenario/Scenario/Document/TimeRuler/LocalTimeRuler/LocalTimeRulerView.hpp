#pragma once

#include <QRect>
#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>

namespace Scenario
{
class LocalTimeRulerView final : public AbstractTimeRulerView
{
public:
  LocalTimeRulerView(QWidget*);
  ~LocalTimeRulerView();

  QRectF boundingRect() const override;
};
}
