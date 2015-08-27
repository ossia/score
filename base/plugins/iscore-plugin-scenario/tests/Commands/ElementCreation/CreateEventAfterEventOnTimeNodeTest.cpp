#include <QtTest/QTest>

#include <Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp>
#include <Commands/Scenario/Creations/CreateEvent.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

#include <Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;


class CreateEventAfterEventOnTimeNodeTest : public QObject
{
        Q_OBJECT
    private slots:
            void CreateTest()
            {
                ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

                EventData data {};
                data.dDate.setMSecs(10);
                data.relativeY = 0.2;

                CreateEvent eventCmd(
                {
                    {"ScenarioModel", {0}},
                }, data);
                eventCmd.redo();

                auto endTn_id = eventCmd.createdTimeNode();

                data.eventClickedId = scenar->startEvent()->id();
                data.relativeY = 0.4;
                data.endTimeNodeId = endTn_id;

                CreateEventAfterEventOnTimeNode cmd(
                {
                    {"ScenarioModel", {0}},
                },  scenar->startEvent()->id(), eventCmd.createdTimeNode(), TimeValue::fromMsecs(10), 0.4);

                cmd.redo();
                QCOMPARE((int) scenar->events().size(), 3 );
                QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.4);

                cmd.undo();
                QCOMPARE((int) scenar->events().size(), 2);
                QCOMPARE((int) scenar->timeNodes().size(), 2);
                try
                {
                    scenar->event(cmd.createdEvent());
                    QFAIL("Event call did not throw!");
                }
                catch(...) { }

                cmd.redo();
                QCOMPARE((int) scenar->events().size(), 3 );
                QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.4);

                delete scenar;
            }
};

QTEST_MAIN(CreateEventAfterEventOnTimeNodeTest)
#include "CreateEventAfterEventOnTimeNodeTest.moc"
