#include <QtTest/QTest>

#include "Commands/TimeNode/SplitTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include <Document/Event/EventData.hpp>
#include "Process/ScenarioModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

class SplitTimeNodeTest: public QObject
{
        Q_OBJECT
    private slots:
        void SplitTest()
        {
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

            data.endTimeNodeId = tn1_id;
            data.relativeY = 0.4;

            CreateEvent event2Cmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            event2Cmd.redo();


            auto event2_id = event2Cmd.createdEvent();
            auto event2 = scenar->event(event2_id);
            auto tn2_id = event2Cmd.createdTimeNode();

            QCOMPARE(tn1_id, tn2_id);

            QVector<Id<EventModel> > evListForNewTn;
            evListForNewTn.push_back(event2_id);

            SplitTimeNode cmd(
            {
                {"TimeNodeModel", {tn1_id.val()}},
            }, evListForNewTn);

            cmd.redo();

            auto newTn_id = cmd.createdTimeNode();
            auto newTn = scenar->timeNode(newTn_id);

            QCOMPARE((int)scenar->timeNodes().size(), 4);

            QCOMPARE(event2->timeNode(), newTn_id);
            QCOMPARE(newTn->events().size(), 1);
            QCOMPARE(newTn->events().at(0), event2_id);

            cmd.undo();
            QCOMPARE((int)scenar->timeNodes().size(), 3);

            QCOMPARE(event2->timeNode(), event1->timeNode());
            try
            {
                scenar->timeNode(newTn_id);
                QFAIL("TimeNode call did not throw !");
            }
            catch(...) { }

            cmd.redo();
            newTn = scenar->timeNode(newTn_id);

            QCOMPARE((int)scenar->timeNodes().size(), 4);

            QCOMPARE(event2->timeNode(), newTn_id);
            QCOMPARE(newTn->events().size(), 1);
            QCOMPARE(newTn->events().at(0), event2_id);

        }
};


QTEST_MAIN(SplitTimeNodeTest)
#include "SplitTimeNodeTest.moc"

