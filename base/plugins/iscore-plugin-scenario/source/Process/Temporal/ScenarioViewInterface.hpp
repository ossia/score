#pragma once

#include <iscore/tools/SettableIdentifier.hpp>
#include <QObject>

class TemporalScenarioPresenter;
class ConstraintModel;
class EventPresenter;
class EventModel;
class TimeNodeModel;
class TimeNodePresenter;
class StatePresenter;

class ScenarioViewInterface : public QObject
{
    public:
        ScenarioViewInterface(TemporalScenarioPresenter* presenter);

        void on_eventMoved(const EventPresenter &event);
        void on_constraintMoved(const id_type<ConstraintModel>& constraintId);
        void on_timeNodeMoved(const TimeNodePresenter &timenode);
        void on_stateMoved(const StatePresenter &state);

    public slots:
        void on_hoverOnConstraint(const id_type<ConstraintModel>& constraintId, bool enter);
        void on_hoverOnEvent(const id_type<EventModel>& eventId, bool enter);

    private:
        TemporalScenarioPresenter* m_presenter;
};
