#pragma once

#include "Document/TimeRuler/AbstractTimeRulerView.hpp"

class LocalTimeRulerView : public AbstractTimeRulerView
{
    public:
        LocalTimeRulerView();
        ~LocalTimeRulerView();

        QRectF boundingRect() const override;

};
