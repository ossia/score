#include <QtTest/QtTest>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>
#include "Control/ProcessList.hpp"

#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/RemoveProcessFromConstraint.hpp"
#include <Process/ScenarioProcessFactory.hpp>

using namespace iscore;
using namespace Scenario::Command;

class RemoveProcessFromConstraintTest: public QObject
{
		Q_OBJECT

	private slots:
		void DeleteCommandTest()
		{
			NamedObject *obj = new NamedObject("obj", qApp);
			ProcessList plist(obj);
			plist.addProcess(new ScenarioProcessFactory);

			ConstraintModel* int_model  = new ConstraintModel{0, qApp};
			int_model->createBox(646);
			ConstraintModel* int_model2 = new ConstraintModel{0, int_model};
			int_model2->createBox(646);

			QVERIFY(int_model2->processes().size() == 0);
			AddProcessToConstraint cmd(
			{
				{"ConstraintModel", {}},
				{"ConstraintModel", 0}
			}, "Scenario");
			cmd.redo();
			QVERIFY(int_model2->processes().size() == 1);

			auto s0 = static_cast<ScenarioProcessSharedModel*>(int_model2->processes().front());

			auto int_0_id = getNextId(s0->constraints());
			auto ev_0_id = getNextId(s0->events());
			s0->createConstraintAndEndEventFromEvent(s0->startEvent()->id(), 55, 10, int_0_id, ev_0_id);
			s0->constraint(int_0_id)->createBox(746);
			QCOMPARE((int)s0->constraints().size(), 1);
			QCOMPARE((int)s0->events().size(), 2); // TODO 3 if endEvent

			AddProcessToConstraint cmd2(
			{
				{"ConstraintModel", {}},
				{"ConstraintModel", 0},
				{"ScenarioProcessSharedModel", s0->id()},
				{"ConstraintModel", int_0_id}
			}, "Scenario");

			cmd2.redo();
			QVERIFY(int_model2->processes().size() == 1);
			auto last_constraint = s0->constraints().front();
			QVERIFY(last_constraint->processes().size() == 1);

			RemoveProcessFromConstraint cmd3(
			{
				{"ConstraintModel", {}},
				{"ConstraintModel", 0}
			}, s0->id());

			cmd3.redo();
			QVERIFY(int_model2->processes().size() == 0);
			cmd3.undo();
			QVERIFY(int_model2->processes().size() == 1);
			cmd3.redo();
			QVERIFY(int_model2->processes().size() == 0);
		}
};

QTEST_MAIN(RemoveProcessFromConstraintTest)
#include "RemoveProcessFromConstraintTest.moc"


