#pragma once

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include <qrect.h>

class LocalTimeRulerView final : public AbstractTimeRulerView
{
    public:
        LocalTimeRulerView();
        ~LocalTimeRulerView();

        QRectF boundingRect() const override;

};
