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
			//ProcessList plist(qApp);
			//plist.addProcess(new );
			
			IntervalModel int_model{0, qApp};
			
			int_model.createContentModel();
			
			AddProcessToIntervalCommand cmd(
			{
				"QApplication",
				{
					{"IntervalModel", 0}
				}
			}, "ScenarioProcess");
			cmd.redo();
		}

};

QTEST_MAIN(AddProcessToIntervalCommandTest)
#include "AddProcessToIntervalCommandTest.moc"

