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

        void on_eventMoved(id_type<EventModel> eventId);
        void on_constraintMoved(id_type<ConstraintModel> constraintId);
        void updateTimeNode(id_type<TimeNodeModel> timeNodeId);

    public slots:
        void on_hoverOnConstraint(id_type<ConstraintModel> constraintId, bool enter);
        void on_hoverOnEvent(id_type<EventModel> eventId, bool enter);

    private:
        TemporalScenarioPresenter* m_presenter;
};
