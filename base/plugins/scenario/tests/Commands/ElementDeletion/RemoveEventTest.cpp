#include <QtTest/QTest>

#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/Scenario/CreateEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include <Document/Event/EventData.hpp>
#include "Process/ScenarioModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

class RemoveEventTest : public QObject
{
        Q_OBJECT
    private slots:
        void RemoveTest()
        {
            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), id_type<ProcessSharedModelInterface> {0}, qApp);

            EventData data {};
            data.dDate.setMSecs(10);
            data.relativeY = 0.4;

            CreateEvent eventCmd(
            {
                {"ScenarioModel", {0}},
            }, data);
            eventCmd.redo();

            auto eventCreated = scenar->event(eventCmd.createdEvent());
            auto event_id = eventCreated->id();
            auto tn_id = eventCreated->timeNode();

            RemoveEvent removeCmd(
            {
                {"ScenarioModel", {0}},
            }, eventCreated );

            removeCmd.redo();
            QCOMPARE((int) scenar->events().size(), 1);
            QCOMPARE((int) scenar->timeNodes().size(), 1);
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
            QCOMPARE((int) scenar->events().size(), 2);
            QCOMPARE((int) scenar->timeNodes().size(), 2);
            QCOMPARE(scenar->event(event_id)->heightPercentage(), 0.4);

            removeCmd.redo();
            QCOMPARE((int) scenar->events().size(), 1);
            QCOMPARE((int) scenar->timeNodes().size(), 1);

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

        }
};


QTEST_MAIN(RemoveEventTest)
#include "RemoveEventTest.moc"
