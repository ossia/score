#include <QtTest/QtTest>
#include <core/QNamedObject>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Document/Process/ScenarioProcessSharedModel.hpp>

#include <Commands/Interval/Process/AddProcessToIntervalCommand.hpp>
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
			IntervalModel model{0, this};
			model.createContentModel();
			auto content = model.contentModel(0);
			QVERIFY(content != nullptr);
			
			content->createStorey();
			auto storey = content->storey(0);
			QVERIFY(storey != nullptr);
		}
		
		void DeleteStoreyTest()
		{
			/////
			IntervalModel model{0, this};
			model.createContentModel();
			auto content = model.contentModel(0);
			
			content->createStorey();
			content->deleteStorey(0);
			model.deleteContentModel(0);
			
			//////
			IntervalModel model2{0, this};
			model2.createContentModel();
			auto content2 = model2.contentModel(0);
			
			content2->createStorey();
			content2->createStorey();
			content2->createStorey();
			model2.deleteContentModel(0);
		}
		
		void FindSubProcessTest()
		{
			IntervalModel i0{0, qApp}; 
			i0.setObjectName("OriginalInterval");
			auto s0 = new ScenarioProcessSharedModel{0, &i0};
			
			s0->createIntervalAndBothEvents(1, 34);
			s0->createIntervalAndBothEvents(42, 46);
			
			auto i1 = s0->interval(0);
			auto s1 = new ScenarioProcessSharedModel{0, i1};
			auto s2 = new ScenarioProcessSharedModel{1, i1};
			
			ObjectPath p{"OriginalInterval", 
						 {
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", 0},
							{"ScenarioProcessSharedModel", 1}
						 }
						};
			QCOMPARE(p.find(), s2);
			
			ObjectPath p2{"OriginalInterval", 
						 {
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", 0},
							{"ScenarioProcessSharedModel", 7}
						 }
						};
			QCOMPARE(p2.find(), static_cast<QObject*>(nullptr));
			
			ObjectPath p3{"OriginalInterval", 
						 {
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel0xBADBAD", 0},
							{"ScenarioProcessSharedModel", 1}
						 }
						};
			QCOMPARE(p3.find(), static_cast<QObject*>(nullptr));
			
			ObjectPath p4{"OriginalInterval", 
						 {
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", 0},
							{"ScenarioProcessSharedModel", 1},
							{"ScenarioProcessSharedModel", 1}
						 }
						};
			QCOMPARE(p4.find(), static_cast<QObject*>(nullptr));
		}

};

QTEST_MAIN(IntervalModelTests)
#include "IntervalModelTests.moc"

