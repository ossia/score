#include <QtTest/QtTest>
#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>


using namespace iscore;
using namespace Scenario::Command;

class RemoveDeckFromBoxTest: public QObject
{
        Q_OBJECT
    public:

    private slots:
        void test()
        {
            ConstraintModel* constraint  = new ConstraintModel {id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, qApp};

            AddBoxToConstraint cmd
            {
                ObjectPath{ {"ConstraintModel", {}} }
            };

            cmd.redo();
            auto box = constraint->box(cmd.m_createdBoxId);

            AddDeckToBox cmd2
            {
                ObjectPath{ {"ConstraintModel", {}},
                    {"BoxModel", box->id() }
                }
            };

            auto deckId = cmd2.m_createdDeckId;
            cmd2.redo();

            RemoveDeckFromBox cmd3
            {
                ObjectPath{ {"ConstraintModel", {}},
                    {"BoxModel", box->id() }
                },
                deckId
            };

            QCOMPARE((int) box->decks().size(), 1);
            cmd3.redo();
            QCOMPARE((int) box->decks().size(), 0);
            cmd3.undo();
            QCOMPARE((int) box->decks().size(), 1);
            cmd2.undo();
            cmd.undo();
            cmd.redo();
            cmd2.redo();
            cmd3.redo();

            QCOMPARE((int) box->decks().size(), 0);






        }
};

QTEST_MAIN(RemoveDeckFromBoxTest)
#include "RemoveDeckFromBoxTest.moc"


