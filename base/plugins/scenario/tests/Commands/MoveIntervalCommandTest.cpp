#include <QtTest/QtTest>

#include <Commands/Scenario/MoveIntervalCommand.hpp>

#include <Document/Interval/IntervalModel.hpp>
#include <Document/Event/EventModel.hpp>

#include <Process/ScenarioProcessSharedModel.hpp>

using namespace iscore;

class MoveIntervalCommandTest: public QObject
{
		Q_OBJECT
	public:

	private slots:

		void MoveCommandTest()
		{
			ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);

			auto int_0_id = getNextId(scenar->intervals());
			auto ev_0_id = getNextId(scenar->events());
			scenar->createIntervalAndEndEventFromStartEvent(10, 0.5, int_0_id, ev_0_id);

			MoveIntervalCommand cmd(
			{
				{"ScenarioProcessSharedModel", {}},
			}, int_0_id, ev_0_id, 0.1 );

			qDebug("\n\n============= Before");
			cmd.redo();
			QCOMPARE(scenar->interval(int_0_id)->heightPercentage(), 0.1);

			qDebug("\n\n============= Undo");
			cmd.undo();
			QCOMPARE(scenar->interval(int_0_id)->heightPercentage(), 0.5);

			qDebug("\n\n============= Redo");
			cmd.redo();
			QCOMPARE(scenar->interval(int_0_id)->heightPercentage(), 0.1);


			// Delete them else they stay in qApp !

			delete scenar;
		}
};

QTEST_MAIN(MoveIntervalCommandTest)
#include "MoveIntervalCommandTest.moc"




