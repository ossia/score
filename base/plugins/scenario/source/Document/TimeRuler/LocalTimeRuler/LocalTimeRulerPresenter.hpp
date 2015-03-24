#pragma once

#include "Document/TimeRuler/AbstractTimeRuler.hpp"
#include "Document/TimeRuler/LocalTimeRuler/LocalTimeRulerView.hpp"

#include "ProcessInterface/TimeValue.hpp"

class LocalTimeRulerPresenter : public AbstractTimeRuler
{
public:
    LocalTimeRulerPresenter(LocalTimeRulerView* view, QObject *parent);
    LocalTimeRulerPresenter(LocalTimeRulerView* view, TimeValue startDate, TimeValue duration, double pixPerMillis, QObject *parent);
    ~LocalTimeRulerPresenter();
};
