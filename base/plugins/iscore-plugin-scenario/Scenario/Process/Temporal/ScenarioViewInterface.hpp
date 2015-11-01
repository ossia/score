#pragma once

#include <iscore/tools/SettableIdentifier.hpp>
#include <QObject>

class TemporalScenarioPresenter;
class TemporalConstraintPresenter;
class EventPresenter;
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class TimeNodePresenter;
class StatePresenter;

class ScenarioViewInterface : public QObject
{
    public:
        ScenarioViewInterface(TemporalScenarioPresenter* presenter);

        void on_eventMoved(const EventPresenter &event);
        void on_constraintMoved(const TemporalConstraintPresenter &constraint);
        void on_timeNodeMoved(const TimeNodePresenter &timenode);
        void on_stateMoved(const StatePresenter &state);

    public slots:
        void on_hoverOnConstraint(const Id<ConstraintModel>& constraintId, bool enter);
        void on_hoverOnEvent(const Id<EventModel>& eventId, bool enter);

    private:
        TemporalScenarioPresenter* m_presenter;
};
