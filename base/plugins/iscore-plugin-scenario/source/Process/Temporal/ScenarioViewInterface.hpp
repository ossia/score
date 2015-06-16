#pragma once

#include <iscore/tools/SettableIdentifier.hpp>
#include <QObject>

class TemporalScenarioPresenter;
class ConstraintModel;
class EventModel;
class TimeNodeModel;


class ScenarioViewInterface : public QObject
{
    public:
        ScenarioViewInterface(TemporalScenarioPresenter* presenter);

        void on_eventMoved(const id_type<EventModel>& eventId);
        void on_constraintMoved(const id_type<ConstraintModel>& constraintId);
        void updateTimeNode(const id_type<TimeNodeModel> &timeNodeId);
        void addPointInTimeNode(const id_type<TimeNodeModel> &timeNodeId, double y);
        void updatePointInTimeNode(const id_type<TimeNodeModel> &timeNodeId, const id_type<ConstraintModel>& cstrId, double newValue);

    public slots:
        void on_hoverOnConstraint(const id_type<ConstraintModel>& constraintId, bool enter);
        void on_hoverOnEvent(const id_type<EventModel>& eventId, bool enter);

    private:
        TemporalScenarioPresenter* m_presenter;
};
