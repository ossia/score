#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>
#include "Control/ProcessList.hpp"

#include <Commands/Constraint/Process/AddProcessToConstraintCommand.hpp>
#include <Commands/Constraint/Process/DeleteProcessFromConstraintCommand.hpp>
#include <Process/ScenarioProcessFactory.hpp>
using namespace iscore;



class AddProcessToConstraintCommandTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void CreateCommandTest()
		{
			NamedObject *obj = new NamedObject{"obj", qApp};
			ProcessList* plist = new ProcessList{obj};
			plist->addProcess(new ScenarioProcessFactory);

			ConstraintModel* int_model  = new ConstraintModel{0, qApp};

			int_model->createBox(646);
			AddProcessToConstraintCommand cmd(
			{
				{"ConstraintModel", {}}
			}, "Scenario");
			cmd.redo();
			cmd.undo();
			cmd.redo();

			// Delete them else they stay in qApp !
			delete int_model;
			delete obj;
		}

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
			AddProcessToConstraintCommand cmd(
			{
				{"ConstraintModel", {}},
				{"ConstraintModel", 0}
			}, "Scenario");
			cmd.redo();
			QVERIFY(int_model2->processes().size() == 1);

			auto s0 = static_cast<ScenarioProcessSharedModel*>(int_model2->processes().front());

			auto int_0_id = getNextId(s0->constraints());
			auto int_1_id = getNextId(s0->constraints());
			auto ev_0_id = getNextId(s0->events());
			auto ev_1_id = getNextId(s0->events());
			s0->createConstraintAndBothEvents(34, 55, 10, int_0_id, ev_0_id, int_1_id, ev_1_id);
			s0->constraint(int_0_id)->createBox(746);
			QVERIFY(s0->constraints().size() == 2);
			QVERIFY(s0->events().size() == 3); // TODO 4 if endEvent

			AddProcessToConstraintCommand cmd2(
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

			DeleteProcessFromConstraintCommand cmd3(
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

QTEST_MAIN(AddProcessToConstraintCommandTest)
#include "AddProcessToConstraintCommandTest.moc"

