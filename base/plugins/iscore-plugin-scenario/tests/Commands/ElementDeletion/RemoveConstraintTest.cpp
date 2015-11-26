#include "QtTest/QTest"

#include <Scenario/Commands/Scenario/Deletions/RemoveConstraint.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEventAfterEvent.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;


class RemoveConstraintTest: public QObject
{
        Q_OBJECT
    private slots:
        void removeFirstConstraintTest()
        {
            // create scenar and 1 event
            Scenario::ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.8;
            data.endTimeNodeId = Id<TimeNodeModel>(-1);

            CreateEvent eventCmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            eventCmd.redo();

            auto event_id = eventCmd.createdEvent();
            auto eventCreated = scenar->event(event_id);

            auto constraint_id = eventCreated->previousConstraints().at(0);
            auto constraint = scenar->constraint(constraint_id);

            auto y = constraint->heightPercentage();

            RemoveConstraint cmd(
            {
                {"ScenarioModel", {0}},
            }, constraint);
            cmd.redo();

            QCOMPARE( (int)scenar->constraints().size(), 0);
            QCOMPARE( (int)eventCreated->previousConstraints().size(), 0);

            cmd.undo();
            QCOMPARE( (int)scenar->constraints().size(), 1);
            QCOMPARE( (int)eventCreated->previousConstraints().size(), 1);
            QCOMPARE( scenar->constraints().at(0)->id(), constraint_id );
            QCOMPARE( eventCreated->previousConstraints().at(0), constraint_id);
            QCOMPARE( scenar->constraint(constraint_id)->heightPercentage(), y );

            cmd.redo();
            QCOMPARE( (int)scenar->constraints().size(), 0);
            QCOMPARE( (int)eventCreated->previousConstraints().size(), 0);


            delete scenar;
        }
        void removeAnyConstraintTest()
        {
            // create scenar and 2 events
            Scenario::ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.8;
            data.endTimeNodeId = Id<TimeNodeModel>(-1);

            CreateEvent eventCmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            eventCmd.redo();

            auto startEv_id = eventCmd.createdEvent();

            CreateEventAfterEvent event2Cmd(
            {
                {"ScenarioModel", {0}},
            }, startEv_id, TimeValue::fromMsecs(30), 0.5);
            event2Cmd.redo();

            auto endEvent_id = event2Cmd.createdEvent();

            auto startEventCreated = scenar->event(startEv_id);
            auto endEventCreated = scenar->event(endEvent_id);

            auto constraint_id = endEventCreated->previousConstraints().at(0);
            auto constraint = scenar->constraint(constraint_id);

            auto y = constraint->heightPercentage();

            RemoveConstraint cmd(
            {
                {"ScenarioModel", {0}},
            }, constraint);
            cmd.redo();

            QCOMPARE( (int)scenar->constraints().size(), 1);
            QCOMPARE( (int)endEventCreated->previousConstraints().size(), 0);
            QCOMPARE( (int)startEventCreated->nextConstraints().size(), 0);

            cmd.undo();
            QCOMPARE( (int)scenar->constraints().size(), 2);
            QCOMPARE( (int)endEventCreated->previousConstraints().size(), 1);
            QCOMPARE( (int)startEventCreated->nextConstraints().size(), 1);

            QCOMPARE( scenar->constraints().at(1)->id(), constraint_id );
            QCOMPARE( endEventCreated->previousConstraints().at(0), constraint_id);
            QCOMPARE( startEventCreated->nextConstraints().at(0), constraint_id);

            QCOMPARE( scenar->constraint(constraint_id)->heightPercentage(), y );

            cmd.redo();
            QCOMPARE( (int)scenar->constraints().size(), 1);
            QCOMPARE( (int)endEventCreated->previousConstraints().size(), 0);
            QCOMPARE( (int)startEventCreated->nextConstraints().size(), 0);


            delete scenar;
        }
};

QTEST_MAIN(RemoveConstraintTest)
#include "RemoveConstraintTest.moc"
