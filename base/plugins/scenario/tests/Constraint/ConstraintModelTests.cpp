#include <QtTest/QtTest>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Process/ScenarioModel.hpp>

#include <Document/TimeNode/TimeNodeModel.hpp>

#include <core/tools/ObjectPath.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>


class ConstraintModelTests: public QObject
{
        Q_OBJECT
    public:
        ConstraintModelTests() : QObject {}
        {
        }

    private slots:

        void CreateDeckTest()
        {
            ConstraintModel model {id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, this};
            auto content_id = getStrongId(model.boxes());
            model.createBox(content_id);
            auto box = model.box(content_id);
            QVERIFY(box != nullptr);

            auto deck_id = getStrongId(box->decks());
            box->addDeck(new DeckModel
            {
                deck_id,
                box
            });
            auto deck = box->deck(deck_id);
            QVERIFY(deck != nullptr);
        }

        void DeleteDeckTest()
        {
            /////
            {
                ConstraintModel model {id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, this};
                auto content_id = getStrongId(model.boxes());
                model.createBox(content_id);
                auto box = model.box(content_id);

                auto deck_id = getStrongId(box->decks());
                box->addDeck(new DeckModel
                {
                    deck_id,
                    box
                });
                box->removeDeck(deck_id);
                model.removeBox(content_id);
            }

            //////
            {
                ConstraintModel model {id_type<ConstraintModel>{0},
                                       id_type<AbstractConstraintViewModel>{0}, this
                                      };
                auto content_id = getStrongId(model.boxes());
                model.createBox(content_id);
                auto box = model.box(content_id);

                box->addDeck(new DeckModel
                {
                    getStrongId(box->decks()),
                    box
                });

                box->addDeck(new DeckModel
                {
                    getStrongId(box->decks()),
                    box
                });

                box->addDeck(new DeckModel
                {
                    getStrongId(box->decks()),
                    box
                });

                model.removeBox(content_id);
            }
        }

        void FindSubProcessTest()
        {
            ConstraintModel i0 {id_type<ConstraintModel>{0},
                                id_type<AbstractConstraintViewModel>{0}, qApp
                               };
            i0.setObjectName("OriginalConstraint");
            auto s0 = new ScenarioModel {id_type<ProcessSharedModelInterface>{0}, &i0};

            auto int_0_id = getStrongId(s0->constraints());
            auto ev_0_id = getStrongId(s0->events());
            auto fv_0_id = id_type<AbstractConstraintViewModel> {234};
            auto tb_0_id = getStrongId(s0->timeNodes());
            s0->createConstraintAndEndEventFromEvent(s0->startEvent()->id(), std::chrono::milliseconds {34}, 10, int_0_id, fv_0_id, ev_0_id);

            auto int_2_id = getStrongId(s0->constraints());
            auto fv_2_id = id_type<AbstractConstraintViewModel> {454};
            auto ev_2_id = getStrongId(s0->events());
            auto tb_2_id = getStrongId(s0->timeNodes());
            s0->createConstraintAndEndEventFromEvent(s0->startEvent()->id(), std::chrono::milliseconds {46}, 10, int_2_id, fv_2_id, ev_2_id);

            auto i1 = s0->constraint(int_0_id);
            auto s1 = new ScenarioModel {id_type<ProcessSharedModelInterface>{0}, i1};
            (void) s1;
            auto s2 = new ScenarioModel {id_type<ProcessSharedModelInterface>{1}, i1};

            ObjectPath p
            {
                {"OriginalConstraint", {}},
                {"ScenarioModel", 0},
                {"ConstraintModel", int_0_id},
                {"ScenarioModel", 1}
            };
            QCOMPARE(p.find<QObject>(), s2);

            ObjectPath p2
            {
                {"OriginalConstraint", {}},
                {"ScenarioModel", 0},
                {"ConstraintModel", int_0_id},
                {"ScenarioModel", 7}
            };

            try
            {
                p2.find<QObject>();
                QFAIL("Exception not thrown");
            }
            catch(...) { }

            ObjectPath p3
            {
                {"OriginalConstraint", {}},
                {"ScenarioModel", 0},
                {"ConstraintModel0xBADBAD", int_0_id},
                {"ScenarioModel", 1}
            };

            try
            {
                p3.find<QObject>();
                QFAIL("Exception not thrown");
            }
            catch(...) { }

            ObjectPath p4
            {
                {"OriginalConstraint", {}},
                {"ScenarioModel", 0},
                {"ConstraintModel", int_0_id},
                {"ScenarioModel", 1},
                {"ScenarioModel", 1}
            };

            try
            {
                p4.find<QObject>();
                QFAIL("Exception not thrown");
            }
            catch(...) { }
        }

};

QTEST_MAIN(ConstraintModelTests)
#include "ConstraintModelTests.moc"

