#pragma once

#include <Document/TimeRuler/AbstractTimeRulerView.hpp>
#include "ProcessInterface/TimeValue.hpp"


class TimeRulerView : public AbstractTimeRulerView
{
    public:
        TimeRulerView();
        QRectF boundingRect() const override;

    signals:

    public slots:

    private:

};
