#include <QtTest/QtTest>

#include <Commands/Scenario/CreateEventAfterEventCommand.hpp>
#include <Commands/Scenario/CreateEventCommand.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>

#include <Process/ScenarioProcessSharedModel.hpp>

using namespace iscore;



class CreateEventAfterEventCommandTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void CreateCommandTest()
        {
            ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);
            CreateEventCommand cmd(
            {
                {"ScenarioProcessSharedModel", {}},
            }, 100, 0.5 );
            int s{0};

            qDebug("\n\n============= CreateEventCommand");
            qDebug("\n============= Before");
            cmd.redo();
            s = scenar->events().size();
            QCOMPARE(s, 2);
            QCOMPARE(scenar->event(1)->heightPercentage(), 0.5);

            qDebug("\n\n============= Undo");
            cmd.undo();
            QCOMPARE(scenar->event(1), static_cast<EventModel*>(nullptr));
            qDebug("\n\n============= Redo");
            cmd.redo();

            s = scenar->events().size();
            QCOMPARE(s, 2);
            QCOMPARE(scenar->event(1)->heightPercentage(), 0.5);


            // Delete them else they stay in qApp !

            delete scenar;
        }

        void CreateAfterCommandTest()
        {
            ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);
            EventData data{};
            data.id = 0;
            data.x = 10;
            data.relativeY = 0.5;

            CreateEventAfterEventCommand cmd(
            {
                {"ScenarioProcessSharedModel", {}},
            }, data );
            int s{0};

            qDebug("\n\n============= CreateEventAfterEventCommand");
            qDebug("\n============= Before");
            cmd.redo();
            s = scenar->events().size();
            QCOMPARE(s, 2);
            QCOMPARE(scenar->event(1)->heightPercentage(), 0.5);

            qDebug("\n\n============= Undo");
            cmd.undo();
            QCOMPARE(scenar->event(1), static_cast<EventModel*>(nullptr));
            qDebug("\n\n============= Redo");
            cmd.redo();

            s = scenar->events().size();
            QCOMPARE(s, 2);
            QCOMPARE(scenar->event(1)->heightPercentage(), 0.5);


            // Delete them else they stay in qApp !

            delete scenar;
        }

};

QTEST_MAIN(CreateEventAfterEventCommandTest)
#include "CreateEventAfterEventCommandTest.moc"


