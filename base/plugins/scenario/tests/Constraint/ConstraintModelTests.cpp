#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Process/ScenarioProcessSharedModel.hpp>

#include <core/tools/ObjectPath.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>


class ConstraintModelTests: public QObject
{
		Q_OBJECT
	public:
		ConstraintModelTests() : QObject{}
		{
		}

	private slots:

		void CreateDeckTest()
		{
			ConstraintModel model{0, this};
			auto content_id = getNextId(model.boxes());
			model.createBox(content_id);
			auto content = model.box(content_id);
			QVERIFY(content != nullptr);

			auto deck_id = getNextId(content->decks());
			content->createDeck(deck_id);
			auto deck = content->deck(deck_id);
			QVERIFY(deck != nullptr);
		}

		void DeleteDeckTest()
		{
			/////
			{
				ConstraintModel model{0, this};
				auto content_id = getNextId(model.boxes());
				model.createBox(content_id);
				auto content = model.box(content_id);

				auto deck_id = getNextId(content->decks());
				content->createDeck(deck_id);
				content->removeDeck(deck_id);
				model.removeBox(content_id);
			}

			//////
			{
				ConstraintModel model{0, this};
				auto content_id = getNextId(model.boxes());
				model.createBox(content_id);
				auto content = model.box(content_id);

				content->createDeck(getNextId(content->decks()));
				content->createDeck(getNextId(content->decks()));
				content->createDeck(getNextId(content->decks()));
				model.removeBox(content_id);
			}
		}

		void FindSubProcessTest()
		{
			ConstraintModel i0{0, qApp};
			i0.setObjectName("OriginalConstraint");
			auto s0 = new ScenarioProcessSharedModel{0, &i0};

			auto int_0_id = getNextId(s0->constraints());
			auto int_1_id = getNextId(s0->constraints());
			auto ev_0_id = getNextId(s0->events());
			auto ev_1_id = getNextId(s0->events());
			s0->createConstraintAndBothEvents(1, 34, 10, int_0_id, ev_0_id, int_1_id, ev_1_id);

			auto int_2_id = getNextId(s0->constraints());
			auto int_3_id = getNextId(s0->constraints());
			auto ev_2_id = getNextId(s0->events());
			auto ev_3_id = getNextId(s0->events());
			s0->createConstraintAndBothEvents(42, 46, 10, int_2_id, ev_2_id, int_3_id, ev_3_id);

			auto i1 = s0->constraint(int_0_id);
			auto s1 = new ScenarioProcessSharedModel{0, i1}; (void) s1;
			auto s2 = new ScenarioProcessSharedModel{1, i1};

			ObjectPath p{
							{"OriginalConstraint", {}},
							{"ScenarioProcessSharedModel", 0},
							{"ConstraintModel", int_0_id},
							{"ScenarioProcessSharedModel", 1}
						 };
			QCOMPARE(p.find(), s2);

			ObjectPath p2{
							{"OriginalConstraint", {}},
							{"ScenarioProcessSharedModel", 0},
							{"ConstraintModel", int_0_id},
							{"ScenarioProcessSharedModel", 7}
						 };
			try
			{
				p2.find();
				QFAIL("Exception not thrown");
			}
			catch(...) { }

			ObjectPath p3{
							{"OriginalConstraint", {}},
							{"ScenarioProcessSharedModel", 0},
							{"ConstraintModel0xBADBAD", int_0_id},
							{"ScenarioProcessSharedModel", 1}
						};
			try
			{
				p3.find();
				QFAIL("Exception not thrown");
			}
			catch(...) { }

			ObjectPath p4{
							{"OriginalConstraint", {}},
							{"ScenarioProcessSharedModel", 0},
							{"ConstraintModel", int_0_id},
							{"ScenarioProcessSharedModel", 1},
							{"ScenarioProcessSharedModel", 1}
						};
			try
			{
				p4.find();
				QFAIL("Exception not thrown");
			}
			catch(...) { }
		}

};

QTEST_MAIN(ConstraintModelTests)
#include "ConstraintModelTests.moc"

