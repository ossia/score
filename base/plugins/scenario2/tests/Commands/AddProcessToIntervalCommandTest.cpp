#include <QtTest/QtTest>
#include <core/QNamedObject>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Document/Process/ScenarioProcessSharedModel.hpp>

#include <Commands/AddProcessToIntervalCommand.hpp>
using namespace iscore;


class AddProcessToIntervalCommandTest: public QObject
{
		Q_OBJECT
	public:

	private slots:


};

QTEST_MAIN(AddProcessToIntervalCommandTest)
#include "AddProcessToIntervalCommandTest.moc"

