#include <QtTest/QtTest>
#include "Commands/Constraint/Box/RemoveSlotFromBox.hpp"
#include "Commands/Constraint/Box/AddSlotToBox.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Slot/SlotModel.hpp>


using namespace iscore;
using namespace Scenario::Command;

class RemoveSlotFromBoxTest: public QObject
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

            AddSlotToBox cmd2
            {
                ObjectPath{ {"ConstraintModel", {}},
                    {"BoxModel", box->id() }
                }
            };

            auto slotId = cmd2.m_createdSlotId;
            cmd2.redo();

            RemoveSlotFromBox cmd3
            {
                ObjectPath{ {"ConstraintModel", {}},
                    {"BoxModel", box->id() }
                },
                slotId
            };

            QCOMPARE((int) box->getSlots().size(), 1);
            cmd3.redo();
            QCOMPARE((int) box->getSlots().size(), 0);
            cmd3.undo();
            QCOMPARE((int) box->getSlots().size(), 1);
            cmd2.undo();
            cmd.undo();
            cmd.redo();
            cmd2.redo();
            cmd3.redo();

            QCOMPARE((int) box->getSlots().size(), 0);






        }
};

QTEST_MAIN(RemoveSlotFromBoxTest)
#include "RemoveSlotFromBoxTest.moc"


