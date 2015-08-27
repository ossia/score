#include <QtTest/QTest>

#include "Commands/Scenario/Deletions/RemoveEvent.hpp"
#include "Commands/Scenario/Creations/CreateEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include <Document/Event/EventData.hpp>
#include "Process/ScenarioModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

class RemoveEventTest : public QObject
{
        Q_OBJECT
    private slots:
        void RemoveEventAndTimeNodeTest()
        {
            // only one event on a timeNode
            // the timeNode will be deleted too

            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.4;
            data.endTimeNodeId = Id<TimeNodeModel>(-1);

            CreateEvent eventCmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            eventCmd.redo();

            auto eventCreated = scenar->event(eventCmd.createdEvent());
            auto event_id = eventCreated->id();
            auto tn_id = eventCreated->timeNode();

            int nbOfEvent = 3;
            int nbOfTimeNodes = 3;

            RemoveEvent removeCmd(
            {
                {"ScenarioModel", {0}},
            }, eventCreated );

            removeCmd.redo();
            QCOMPARE((int) scenar->events().size(), nbOfEvent - 1);
            QCOMPARE((int) scenar->timeNodes().size(), nbOfTimeNodes - 1);
            try
            {
                scenar->event(event_id);
                QFAIL("Event call did not throw!");
            }
            catch(...) { }
            try
            {
                scenar->timeNode(tn_id);
                QFAIL("TimeNode call did not throw!");
            }
            catch(...) { }

            removeCmd.undo();
            QCOMPARE((int) scenar->events().size(), nbOfEvent );
            QCOMPARE((int) scenar->timeNodes().size(), nbOfTimeNodes );
            QCOMPARE(scenar->event(event_id)->heightPercentage(), 0.4);

            removeCmd.redo();
            QCOMPARE((int) scenar->events().size(), nbOfEvent - 1);
            QCOMPARE((int) scenar->timeNodes().size(), nbOfTimeNodes - 1);

            try
            {
                scenar->event(event_id);
                QFAIL("Event call did not throw!");
            }
            catch(...) { }
            try
            {
                scenar->timeNode(tn_id);
                QFAIL("TimeNode call did not throw!");
            }
            catch(...) { }

            delete scenar;

        }

        void RemoveOnlyEventTest()
        {
            // two events on a same timeNode
            // test removing just one of them : the timeNode stay

            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.8;
            data.endTimeNodeId = Id<TimeNodeModel>(-1);

            CreateEvent eventCmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            eventCmd.redo();

            data.endTimeNodeId = eventCmd.createdTimeNode();
            data.relativeY = 0.4;

            CreateEvent event2Cmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            event2Cmd.redo();

            auto event_id = event2Cmd.createdEvent();
            auto eventCreated = scenar->event(event_id);

            int prevConstraintCount = 1;
            QCOMPARE (eventCreated->previousConstraints().size(), prevConstraintCount);

            auto prevConstraints = eventCreated->previousConstraints();

            int nbOfEvent = 4;
            int nbOfTimeNodes = 3;

            RemoveEvent removeCmd(
            {
                {"ScenarioModel", {0}},
            }, eventCreated );

            removeCmd.redo();
            QCOMPARE((int) scenar->events().size(), nbOfEvent - 1);
            QCOMPARE((int) scenar->timeNodes().size(), nbOfTimeNodes);
            try
            {
                scenar->event(event_id);
                QFAIL("Event call did not throw!");
            }
            catch(...) { }

            removeCmd.undo();
            QCOMPARE((int) scenar->events().size(), nbOfEvent);
            QCOMPARE((int) scenar->timeNodes().size(), nbOfTimeNodes);
            QCOMPARE(scenar->event(event_id)->heightPercentage(), 0.4);
            QCOMPARE(scenar->event(event_id)->previousConstraints().size(), prevConstraintCount);
            QCOMPARE(scenar->event(event_id)->previousConstraints().first(), prevConstraints[0] );

            removeCmd.redo();
            QCOMPARE((int) scenar->events().size(), nbOfEvent - 1);
            QCOMPARE((int) scenar->timeNodes().size(), nbOfTimeNodes);

            try
            {
                scenar->event(event_id);
                QFAIL("Event call did not throw!");
            }
            catch(...) { }

            delete scenar;
        }

};


QTEST_MAIN(RemoveEventTest)
#include "RemoveEventTest.moc"
