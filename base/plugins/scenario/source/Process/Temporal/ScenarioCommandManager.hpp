#pragma once
#include <QObject>
#include <core/presenter/command/OngoingCommandManager.hpp>
#include <Document/Event/EventData.hpp>
class TemporalScenarioPresenter;

namespace iscore
{
    class SerializableCommand;
}

struct ConstraintData;
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class QPointF;

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
        EventData m_lastData {};

        TemporalScenarioPresenter* m_presenter{};
        OngoingCommandDispatcher<MergeStrategy::Undo>* m_creationCommandDispatcher{};
        OngoingCommandDispatcher<MergeStrategy::Simple>* m_moveCommandDispatcher{};
        CommandDispatcher<SendStrategy::Simple>* m_instantCommandDispatcher{};
};
