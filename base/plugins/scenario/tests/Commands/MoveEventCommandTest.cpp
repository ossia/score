#include <QtTest/QtTest>

#include <Commands/Scenario/MoveEventCommand.hpp>

#include <Document/Event/EventModel.hpp>
#include <Document/Event/EventData.hpp>

#include <Process/ScenarioProcessSharedModel.hpp>

using namespace iscore;

class MoveEventCommandTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void MoveCommandTest()
        {
            ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);
            EventData data{};
            data.id = 0;
            data.x = 10;
            data.relativeY = 0.1;

            MoveEventCommand cmd(
            {
                {"ScenarioProcessSharedModel", {}},
            }, data );

            qDebug("\n============= Before");
            cmd.redo();
            QCOMPARE(scenar->event(0)->heightPercentage(), 0.1);

            qDebug("\n\n============= Undo");
            cmd.undo();
            QCOMPARE(scenar->event(0)->heightPercentage(), 0.5);

            qDebug("\n\n============= Redo");
            cmd.redo();
            QCOMPARE(scenar->event(0)->heightPercentage(), 0.1);


            // Delete them else they stay in qApp !

            delete scenar;
        }
};

QTEST_MAIN(MoveEventCommandTest)
#include "MoveEventCommandTest.moc"



