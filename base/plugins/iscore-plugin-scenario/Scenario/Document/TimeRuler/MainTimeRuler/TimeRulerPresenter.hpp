#pragma once

#include <Scenario/Document/TimeRuler/AbstractTimeRuler.hpp>

class QObject;

namespace Scenario
{
class TimeRulerView;

class TimeRulerPresenter final : public AbstractTimeRuler
{
        Q_OBJECT
    public:
        explicit TimeRulerPresenter(TimeRulerView* view, QObject *parent = 0);
        ~TimeRulerPresenter();

        TimeRulerView* view();

};
}
