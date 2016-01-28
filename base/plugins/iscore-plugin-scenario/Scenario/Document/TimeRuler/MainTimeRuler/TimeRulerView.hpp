#pragma once

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include <QRect>

namespace Scenario
{
class TimeRulerView final : public AbstractTimeRulerView
{
    public:
        TimeRulerView();
        QRectF boundingRect() const override;

};
}
