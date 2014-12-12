#include <QtTest/QtTest>

#include <Commands/Scenario/MoveEventCommand.hpp>

#include <Document/Event/EventModel.hpp>

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

			MoveEventCommand cmd(
			{
				{"ScenarioProcessSharedModel", {}},
			}, scenar->startEvent()->id(), 10, 0.1 );

			qDebug("\n============= Before");
			cmd.redo();
			QCOMPARE(scenar->startEvent()->heightPercentage(), 0.1);

			qDebug("\n\n============= Undo");
			cmd.undo();
			QCOMPARE(scenar->startEvent()->heightPercentage(), 0.5);

			qDebug("\n\n============= Redo");
			cmd.redo();
			QCOMPARE(scenar->startEvent()->heightPercentage(), 0.1);


			// Delete them else they stay in qApp !

			delete scenar;
		}
};

QTEST_MAIN(MoveEventCommandTest)
#include "MoveEventCommandTest.moc"



