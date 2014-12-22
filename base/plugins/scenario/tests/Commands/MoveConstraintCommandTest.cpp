#include <QtTest/QtTest>

#include <Commands/Scenario/MoveConstraintCommand.hpp>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/ConstraintData.hpp>
#include <Document/Event/EventModel.hpp>

#include <Process/ScenarioProcessSharedModel.hpp>

using namespace iscore;

class MoveConstraintCommandTest: public QObject
{
		Q_OBJECT
	public:

	private slots:

		void MoveCommandTest()
		{
			ScenarioProcessSharedModel* scenar = new ScenarioProcessSharedModel(0, qApp);

			auto int_0_id = getNextId(scenar->constraints());
			auto ev_0_id = getNextId(scenar->events());
			scenar->createConstraintAndEndEventFromStartEvent(10, 0.5, int_0_id, ev_0_id);

			ConstraintData data{};
			data.id = int_0_id;
			data.relativeY = 0.1;
			MoveConstraintCommand cmd(
			{
				{"ScenarioProcessSharedModel", {}},
			}, data );

			cmd.redo();
			QCOMPARE(scenar->constraint(int_0_id)->heightPercentage(), 0.1);

			cmd.undo();
			QCOMPARE(scenar->constraint(int_0_id)->heightPercentage(), 0.5);

			cmd.redo();
			QCOMPARE(scenar->constraint(int_0_id)->heightPercentage(), 0.1);


			// Delete them else they stay in qApp !

			delete scenar;
		}
};

QTEST_MAIN(MoveConstraintCommandTest)
#include "MoveConstraintCommandTest.moc"




