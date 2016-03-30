#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ConstraintModel;
class EventModel;
class EventPresenter;
class StatePresenter;
class TemporalConstraintPresenter;
class TemporalScenarioPresenter;
class TimeNodePresenter;
class CommentBlockPresenter;
class ScenarioViewInterface
{
    public:
        ScenarioViewInterface(const TemporalScenarioPresenter& presenter);

        void on_eventMoved(const EventPresenter &event);
        void on_constraintMoved(const TemporalConstraintPresenter &constraint);
        void on_timeNodeMoved(const TimeNodePresenter &timenode);
        void on_stateMoved(const StatePresenter &state);
        void on_commentMoved(const CommentBlockPresenter& comment);

        void on_hoverOnConstraint(const Id<ConstraintModel>& constraintId, bool enter);
        void on_hoverOnEvent(const Id<EventModel>& eventId, bool enter);

        void on_graphicalScaleChanged(double scale);

    private:
        const TemporalScenarioPresenter& m_presenter;
};
}
