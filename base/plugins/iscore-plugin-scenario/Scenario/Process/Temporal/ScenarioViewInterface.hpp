#pragma once
class ConstraintModel;
class EventModel;
class EventPresenter;
class StatePresenter;
class TemporalConstraintPresenter;
class TemporalScenarioPresenter;
class TimeNodePresenter;
#include <iscore/tools/SettableIdentifier.hpp>

class ScenarioViewInterface
{
    public:
        ScenarioViewInterface(const TemporalScenarioPresenter& presenter);

        void on_eventMoved(const EventPresenter &event);
        void on_constraintMoved(const TemporalConstraintPresenter &constraint);
        void on_timeNodeMoved(const TimeNodePresenter &timenode);
        void on_stateMoved(const StatePresenter &state);

        void on_hoverOnConstraint(const Id<ConstraintModel>& constraintId, bool enter);
        void on_hoverOnEvent(const Id<EventModel>& eventId, bool enter);

    private:
        const TemporalScenarioPresenter& m_presenter;
};
