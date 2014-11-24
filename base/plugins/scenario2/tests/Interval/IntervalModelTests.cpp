#include <QtTest/QtTest>
#include <core/QNamedObject>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
using namespace iscore;


class IntervalModelTests: public QObject
{
		Q_OBJECT
	public:
		IntervalModelTests() : QObject{}
		{
		}

	private slots:
		void CreateStoreyTest()
		{
			IntervalModel model{nullptr, nullptr, 0, this};
			model.createContentModel();
			auto content = model.contentModel(0);
			QVERIFY(content != nullptr);
			
			content->createStorey();
			auto storey = content->storey(0);
			QVERIFY(storey != nullptr);
		}

};

QTEST_MAIN(IntervalModelTests)
#include "IntervalModelTests.moc"

