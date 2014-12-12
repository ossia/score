#include <QtTest/QtTest>
#include <Document/Interval/IntervalModel.hpp>
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
			new IntervalModel{0, int_model};

			AddProcessToIntervalCommand cmd(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0}
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

			AddProcessToIntervalCommand cmd(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0}
			}, "Scenario");
			cmd.redo();

			auto scen_model = static_cast<ScenarioProcessSharedModel*>(int_model2->process(0));
			scen_model->createIntervalAndBothEvents(34, 55, 10);
			AddProcessToIntervalCommand cmd2(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0},
				{"ScenarioProcessSharedModel", 0},
				{"IntervalModel", 0}
			}, "Scenario");
			cmd2.redo();


			DeleteProcessFromIntervalCommand cmd3(
			{
				{"IntervalModel", {}},
				{"IntervalModel", 0}
			}, "ScenarioProcessSharedModel", 0);


			qDebug("\n\n\n============= Before");
			int_model->dumpObjectTree();
			cmd3.redo();
			qDebug("\n\n\n============= Redo");
			int_model->dumpObjectTree();
			cmd3.undo();
			qDebug("\n\n\n============= Undo");
			int_model->dumpObjectTree();
			cmd3.redo();
		}

};

QTEST_MAIN(AddProcessToIntervalCommandTest)
#include "AddProcessToIntervalCommandTest.moc"

