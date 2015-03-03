#pragma once
#include <QObject>
class TemporalScenarioPresenter;

namespace iscore
{
    class SerializableCommand;
}

struct EventData;
struct ConstraintData;
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class QPointF;
class OngoingCommandManager;

class ScenarioCommandManager : public QObject
{
    public:
        ScenarioCommandManager(TemporalScenarioPresenter* presenter);

        void setupEventPresenter(EventPresenter* e);
        void setupTimeNodePresenter(TimeNodePresenter* t);
        void setupConstraintPresenter(TemporalConstraintPresenter* c);

        void createConstraint(EventData);
        void on_scenarioReleased(QPointF point, QPointF scenePoint);

        void clearContentFromSelection();
        void deleteSelection();

        // Moving
        void moveEventAndConstraint(EventData data);
        void moveConstraint(ConstraintData data);
        void moveTimeNode(EventData data);

        void on_ctrlStateChanged(bool);

        // Utility
        bool ongoingCommand();

    private:
        TemporalScenarioPresenter* m_presenter{};
        OngoingCommandManager* m_commandManager{};
};
