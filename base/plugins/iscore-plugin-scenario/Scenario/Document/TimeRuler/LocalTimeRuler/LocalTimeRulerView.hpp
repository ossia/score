#pragma once

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>

class LocalTimeRulerView final : public AbstractTimeRulerView
{
    public:
        LocalTimeRulerView();
        ~LocalTimeRulerView();

        QRectF boundingRect() const override;

};
