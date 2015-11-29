#pragma once

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include <QRect>


class TimeRulerView final : public AbstractTimeRulerView
{
    public:
        TimeRulerView();
        QRectF boundingRect() const override;

};
