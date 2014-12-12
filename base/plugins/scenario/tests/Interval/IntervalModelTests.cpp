#include <QtTest/QtTest>
#include <Document/Interval/IntervalModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Interval/IntervalContent/IntervalContentModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>

#include <core/tools/ObjectPath.hpp>
#include <Document/Interval/IntervalContent/Storey/PositionedStorey/PositionedStoreyModel.hpp>


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
			auto content_id = getNextId(model.contentModels());
			model.createContentModel(content_id);
			auto content = model.contentModel(content_id);
			QVERIFY(content != nullptr);

			auto storey_id = getNextId(content->storeys());
			content->createStorey(storey_id);
			auto storey = content->storey(storey_id);
			QVERIFY(storey != nullptr);
		}

		void DeleteStoreyTest()
		{
			/////
			{
				IntervalModel model{0, this};
				auto content_id = getNextId(model.contentModels());
				model.createContentModel(content_id);
				auto content = model.contentModel(content_id);

				auto storey_id = getNextId(content->storeys());
				content->createStorey(storey_id);
				content->deleteStorey(storey_id);
				model.deleteContentModel(content_id);
			}

			//////
			{
				IntervalModel model{0, this};
				auto content_id = getNextId(model.contentModels());
				model.createContentModel(content_id);
				auto content = model.contentModel(content_id);

				content->createStorey(getNextId(content->storeys()));
				content->createStorey(getNextId(content->storeys()));
				content->createStorey(getNextId(content->storeys()));
				model.deleteContentModel(content_id);
			}
		}

		void FindSubProcessTest()
		{
			IntervalModel i0{0, qApp};
			i0.setObjectName("OriginalInterval");
			auto s0 = new ScenarioProcessSharedModel{0, &i0};

			auto int_0_id = getNextId(s0->intervals());
			auto int_1_id = getNextId(s0->intervals());
			auto ev_0_id = getNextId(s0->events());
			auto ev_1_id = getNextId(s0->events());
			s0->createIntervalAndBothEvents(1, 34, 10, int_0_id, ev_0_id, int_1_id, ev_1_id);

			auto int_2_id = getNextId(s0->intervals());
			auto int_3_id = getNextId(s0->intervals());
			auto ev_2_id = getNextId(s0->events());
			auto ev_3_id = getNextId(s0->events());
			s0->createIntervalAndBothEvents(42, 46, 10, int_2_id, ev_2_id, int_3_id, ev_3_id);

			auto i1 = s0->interval(int_0_id);
			auto s1 = new ScenarioProcessSharedModel{0, i1};
			auto s2 = new ScenarioProcessSharedModel{1, i1};

			ObjectPath p{
							{"OriginalInterval", {}},
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", int_0_id},
							{"ScenarioProcessSharedModel", 1}
						 };
			QCOMPARE(p.find(), s2);

			ObjectPath p2{
							{"OriginalInterval", {}},
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", int_0_id},
							{"ScenarioProcessSharedModel", 7}
						 };
			QCOMPARE(p2.find(), static_cast<QObject*>(nullptr));

			ObjectPath p3{
							{"OriginalInterval", {}},
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel0xBADBAD", int_0_id},
							{"ScenarioProcessSharedModel", 1}
						};
			QCOMPARE(p3.find(), static_cast<QObject*>(nullptr));

			ObjectPath p4{
							{"OriginalInterval", {}},
							{"ScenarioProcessSharedModel", 0},
							{"IntervalModel", int_0_id},
							{"ScenarioProcessSharedModel", 1},
							{"ScenarioProcessSharedModel", 1}
						};
			QCOMPARE(p4.find(), static_cast<QObject*>(nullptr));
		}

};

QTEST_MAIN(IntervalModelTests)
#include "IntervalModelTests.moc"

