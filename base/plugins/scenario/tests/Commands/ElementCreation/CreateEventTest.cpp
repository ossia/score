#include <QtTest/QtTest>

#include <Commands/Scenario/Creations/CreateEvent.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>

#include <Process/ScenarioModel.hpp>

using namespace iscore;
using namespace Scenario::Command;


class CreateEventTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void CreateTest()
        {
            ScenarioModel* scenar = new ScenarioModel(std::chrono::seconds(15), id_type<ProcessSharedModelInterface> {0}, qApp);
            EventData data {};
            // data.id = 0; unused here
            data.dDate.setMSecs(10);
            data.relativeY = 0.2;

            CreateEvent cmd(
            {
                {"ScenarioModel", {0}},
            }, data);

            cmd.redo();
            QCOMPARE((int) scenar->events().size(), 3);
            QCOMPARE(scenar->event(cmd.m_cmd->m_createdEventId)->heightPercentage(), 0.2);

            cmd.undo();
            QCOMPARE((int) scenar->events().size(), 2);

            cmd.redo();
            QCOMPARE((int) scenar->events().size(), 2);
            QCOMPARE(scenar->event(cmd.m_cmd->m_createdEventId)->heightPercentage(), 0.2);


            // Delete them else they stay in qApp !

            delete scenar;
        }
};

QTEST_MAIN(CreateEventTest)
#include "CreateEventTest.moc"


