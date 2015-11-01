#include <QtTest/QTest>

#include <Scenario/Commands/TimeNode/MergeTimeNodes.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

class MergeTimeNodesTest: public QObject
{
        Q_OBJECT
    private slots:
        void mergeTest()
        {
            // A scenario, 2 events at same date
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

            auto event1_id = eventCmd.createdEvent();
            auto event1 = scenar->event(event1_id);
            auto tn1_id = eventCmd.createdTimeNode();

            data.relativeY = 0.4;

            CreateEvent event2Cmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            event2Cmd.redo();

            auto event2_id = event2Cmd.createdEvent();
            auto event2 = scenar->event(event2_id);
            auto tn2_id = event2Cmd.createdTimeNode();

            auto tn2 = scenar->timeNode(tn2_id);

            tn2->addEvent(event1_id);

            MergeTimeNodes cmd(
            {
                {"ScenarioModel", {0}},
            }, tn1_id, tn2_id);

            cmd.redo();
            QCOMPARE( (int)scenar->timeNodes().size(), 3);

            QCOMPARE( event2->timeNode(), event1->timeNode());
            try
            {
                scenar->timeNode(tn2_id);
                QFAIL("TimeNode call did not throw");
            }
            catch(...) { }

            cmd.undo();
            QCOMPARE( (int)scenar->timeNodes().size(), 4);
            try
            {
                scenar->timeNode(tn2_id);
                QCOMPARE(scenar->timeNode(tn2_id)->events().first(), event2_id);
            }
            catch(...) {QFAIL("TimeNode call throw ..."); }

            cmd.redo();
            QCOMPARE( (int)scenar->timeNodes().size(), 3);
            QCOMPARE( event2->timeNode(), event1->timeNode());
            try
            {
                scenar->timeNode(tn2_id);
                QFAIL("TimeNode call did not throw");
            }
            catch(...) { }


        }
};

QTEST_MAIN(MergeTimeNodesTest)
#include "MergeTimeNodesTest.moc"
