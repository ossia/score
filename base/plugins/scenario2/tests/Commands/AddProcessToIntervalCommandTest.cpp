#include <QtTest/QtTest>
#include <core/QNamedObject>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Document/Process/ScenarioProcessSharedModel.hpp>
#include <core/processes/ProcessList.hpp>

#include <Commands/Interval/Process/AddProcessToIntervalCommand.hpp>
#include <Process/ScenarioProcessFactory.hpp>
using namespace iscore;



class AddProcessToIntervalCommandTest: public QObject
{
		Q_OBJECT
	public:

	private slots:
		void CreateCommandTest()
		{
			QNamedObject *obj = new QNamedObject(qApp, "obj");
			ProcessList plist(obj); 
			plist.addProcess(new ScenarioProcessFactory);
			
			IntervalModel* int_model  = new IntervalModel{0, qApp};
			IntervalModel* int_model2 = new IntervalModel{0, int_model};
			
			AddProcessToIntervalCommand cmd(
			{
				"IntervalModel",
				{
					{"IntervalModel", 0}
				}
			}, "Scenario");
			cmd.redo();
			cmd.undo();
			cmd.redo();
		}

};

QTEST_MAIN(AddProcessToIntervalCommandTest)
#include "AddProcessToIntervalCommandTest.moc"

