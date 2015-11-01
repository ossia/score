#include <QtTest/QtTest>

#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventData.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;


class CreateEventTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void CreateTest()
        {
            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);
            EventData data {};
            // data.id = 0; unused here
            data.dDate.setMSecs(10);
            data.relativeY = 0.2;
            data.endTimeNodeId = Id<TimeNodeModel>(-1);

            CreateEvent cmd(
            {
                {"ScenarioModel", {0}},
            }, data);

            cmd.redo();

            auto event = scenar->event(cmd.createdEvent());

            QCOMPARE((int) scenar->events().size(), 3);
            QCOMPARE(event->heightPercentage(), 0.2);
            QCOMPARE( cmd.createdTimeNode(), event->timeNode() );

            cmd.undo();
            QCOMPARE((int) scenar->events().size(), 2);
            scenar->event(Id<EventModel>(0));
            scenar->event(Id<EventModel>(1));

            cmd.redo();
            QCOMPARE((int) scenar->events().size(), 3);
            QCOMPARE(event->heightPercentage(), 0.2);


            // Delete them else they stay in qApp !

            delete scenar;
        }
        void CreateOnTimeNodeTest()
        {
            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);
            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.6;
            data.endTimeNodeId = Id<TimeNodeModel>(-1);

            CreateEvent cmd1(
            {
                {"ScenarioModel", {0}},
            }, data);
            cmd1.redo();

            data.endTimeNodeId = cmd1.createdTimeNode();
            data.relativeY = 0.2;

            CreateEvent cmd(
            {
                {"ScenarioModel", {0}},
            }, data);

            int eventCount = 4;
            int timeNodeCount = 3;

            cmd.redo();
            QCOMPARE((int) scenar->events().size(), eventCount);
            QCOMPARE((int) scenar->timeNodes().size(), timeNodeCount);
            QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.2);

            cmd.undo();
            QCOMPARE((int) scenar->events().size(), eventCount - 1);
            QCOMPARE((int) scenar->timeNodes().size(), timeNodeCount);

            cmd.redo();
            QCOMPARE((int) scenar->events().size(), eventCount);
            QCOMPARE((int) scenar->timeNodes().size(), timeNodeCount);
            QCOMPARE(scenar->event(cmd.createdEvent())->heightPercentage(), 0.2);


            // Delete them else they stay in qApp !

            delete scenar;
        }
};

QTEST_MAIN(CreateEventTest)
#include "CreateEventTest.moc"


