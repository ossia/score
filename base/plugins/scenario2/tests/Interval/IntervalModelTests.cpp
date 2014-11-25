#include <QtTest/QtTest>
#include <core/QNamedObject>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Document/Process/ScenarioProcessSharedModel.hpp>

#include <Commands/AddProcessToIntervalCommand.hpp>
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
		
		void DeleteStoreyTest()
		{
			/////
			IntervalModel model{nullptr, nullptr, 0, this};
			model.createContentModel();
			auto content = model.contentModel(0);
			
			content->createStorey();
			content->deleteStorey(0);
			model.deleteContentModel(0);
			
			//////
			IntervalModel model2{nullptr, nullptr, 0, this};
			model2.createContentModel();
			auto content2 = model2.contentModel(0);
			
			content2->createStorey();
			content2->createStorey();
			content2->createStorey();
			model2.deleteContentModel(0);
		}
		
		void FindSubProcessTest()
		{
			IntervalModel i0{nullptr, nullptr, 0, qApp}; i0.setObjectName("dadada");
			auto s0 = new ScenarioProcessSharedModel{0, &i0};
			
			s0->createInterval(34);
			s0->createInterval(42);
			
			auto i1 = s0->interval(0);
			auto s1 = new ScenarioProcessSharedModel{0, i1};
			auto s2 = new ScenarioProcessSharedModel{1, i1};
			
			ObjectPath p{"dadada", 
						 {
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", 0},
							{"ScenarioProcessSharedModel", 1}
						 }
						};
			auto obj = p.find();
			QCOMPARE(obj, s2);
			
			
			
		}

};

QTEST_MAIN(IntervalModelTests)
#include "IntervalModelTests.moc"

