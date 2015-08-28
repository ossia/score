#include <QtTest/QtTest>

#include <Commands/Scenario/Creations/CreateEventAfterEvent.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>

#include <Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;


class CreateEventAfterEventTest: public QObject
{
        Q_OBJECT
    public:

    private slots:
        void CreateTest()
        {
            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), Id<ProcessModel> {0}, qApp);

            CreateEventAfterEvent cmd(
            {
                {"ScenarioModel", {0}},
            }, scenar->startEvent()->id(), TimeValue::fromMsecs(10), 0.5);

            cmd.redo();
            QCOMPARE((int) scenar->events().size(), 2);
            QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);

            cmd.undo();
            QCOMPARE((int) scenar->events().size(), 1);

            try
            {
                scenar->event(cmd.m_createdEventId);
                QFAIL("Event call did not throw!");
            }
            catch(...) { }

            cmd.redo();

            QCOMPARE((int) scenar->events().size(), 2);
            QCOMPARE(scenar->event(cmd.m_createdEventId)->heightPercentage(), 0.5);


            // Delete them else they stay in qApp !

            delete scenar;
        }
};

QTEST_MAIN(CreateEventAfterEventTest)
#include "CreateEventAfterEventTest.moc"


