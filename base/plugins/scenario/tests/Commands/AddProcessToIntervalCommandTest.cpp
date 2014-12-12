#include <QtTest/QtTest>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>
#include "Control/ProcessList.hpp"

#include <Commands/Interval/Process/AddProcessToIntervalCommand.hpp>
#include <Commands/Interval/Process/DeleteProcessFromIntervalCommand.hpp>
#include <Process/ScenarioProcessFactory.hpp>
using namespace iscore;



class AddProcessToIntervalCommandTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void CreateCommandTest()
		{
			NamedObject *obj = new NamedObject{"obj", qApp};
			ProcessList* plist = new ProcessList{obj};
			plist->addProcess(new ScenarioProcessFactory);

			IntervalModel* int_model  = new IntervalModel{0, qApp};
			AddProcessToIntervalCommand cmd(
			{
				{"IntervalModel", {}}
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

			IntervalModel* int_model  = new IntervalModel{0, qApp};
			IntervalModel* int_model2 = new IntervalModel{0, int_model};

			QVERIFY(int_model2->processes().size() == 0);
			AddProcessToIntervalCommand cmd(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0}
			}, "Scenario");
			cmd.redo();
			QVERIFY(int_model2->processes().size() == 1);

			auto s0 = static_cast<ScenarioProcessSharedModel*>(int_model2->processes().front());

			auto int_0_id = getNextId(s0->intervals());
			auto int_1_id = getNextId(s0->intervals());
			auto ev_0_id = getNextId(s0->events());
			auto ev_1_id = getNextId(s0->events());
			s0->createIntervalAndBothEvents(34, 55, 10, int_0_id, ev_0_id, int_1_id, ev_1_id);
			QVERIFY(s0->intervals().size() == 2);
			QVERIFY(s0->events().size() == 3); // TODO 4 if endEvent

			AddProcessToIntervalCommand cmd2(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0},
				{"ScenarioProcessSharedModel", s0->id()},
				{"IntervalModel", int_0_id}
			}, "Scenario");

			cmd2.redo();
			QVERIFY(int_model2->processes().size() == 1);
			auto last_interval = s0->intervals().front();
			QVERIFY(last_interval->processes().size() == 1);

			DeleteProcessFromIntervalCommand cmd3(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0}
			}, "ScenarioProcessSharedModel", s0->id());

			cmd3.redo();
			QVERIFY(int_model2->processes().size() == 0);
			cmd3.undo();
			QVERIFY(int_model2->processes().size() == 1);
			cmd3.redo();
			QVERIFY(int_model2->processes().size() == 0);
		}

};

QTEST_MAIN(AddProcessToIntervalCommandTest)
#include "AddProcessToIntervalCommandTest.moc"

