#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Document/TimeRuler/AbstractTimeRuler.hpp>

class LocalTimeRulerView;
class QObject;

//TODO : total refactor => unused now

class LocalTimeRulerPresenter final : public AbstractTimeRuler
{
    public:
        LocalTimeRulerPresenter(LocalTimeRulerView* view, QObject *parent);
        LocalTimeRulerPresenter(LocalTimeRulerView* view, TimeValue startDate, TimeValue duration, double pixPerMillis, QObject *parent);
        ~LocalTimeRulerPresenter();

        void setStartPoint(TimeValue dur) override;
};
