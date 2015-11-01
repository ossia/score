#pragma once

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include <Process/TimeValue.hpp>


class TimeRulerView : public AbstractTimeRulerView
{
    public:
        TimeRulerView();
        QRectF boundingRect() const override;

    signals:

    public slots:

    private:

};
