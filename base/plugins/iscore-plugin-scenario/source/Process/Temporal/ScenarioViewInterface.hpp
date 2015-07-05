#pragma once

#include <iscore/tools/SettableIdentifier.hpp>
#include <QObject>

class TemporalScenarioPresenter;
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class TimeNodePresenter;
class DisplayedStateModel;

class ScenarioViewInterface : public QObject
{
    public:
        ScenarioViewInterface(TemporalScenarioPresenter* presenter);

        void on_eventMoved(const id_type<EventModel>& eventId);
        void on_constraintMoved(const id_type<ConstraintModel>& constraintId);
        void on_timeNodeMoved(const TimeNodePresenter &timenode);
        void on_stateMoved(const id_type<DisplayedStateModel>& constraintId);

        void updateTimeNode(const id_type<TimeNodeModel> &timeNodeId);


        void addPointInEvent(const id_type<EventModel> &eventId, double y);

    public slots:
        void on_hoverOnConstraint(const id_type<ConstraintModel>& constraintId, bool enter);
        void on_hoverOnEvent(const id_type<EventModel>& eventId, bool enter);

    private:
        TemporalScenarioPresenter* m_presenter;
};
