#include <QtTest/QtTest>
#include <Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>


using namespace iscore;
using namespace Scenario::Command;

class RemoveSlotFromRackTest: public QObject
{
        Q_OBJECT
    public:

    private slots:
        void test()
        {
            ConstraintModel* constraint  = new ConstraintModel {Id<ConstraintModel>{0}, Id<ConstraintViewModel>{0}, qApp};

            AddRackToConstraint cmd
            {
                ObjectPath{ {"ConstraintModel", {}} }
            };

            cmd.redo();
            auto rack = constraint->rack(cmd.m_createdRackId);

            AddSlotToRack cmd2
            {
                ObjectPath{ {"ConstraintModel", {}},
                    {"RackModel", rack->id() }
                }
            };

            auto slotId = cmd2.m_createdSlotId;
            cmd2.redo();

            RemoveSlotFromRack cmd3
            {
                ObjectPath{ {"ConstraintModel", {}},
                    {"RackModel", rack->id() }
                },
                slotId
            };

            QCOMPARE((int) rack->getSlots().size(), 1);
            cmd3.redo();
            QCOMPARE((int) rack->getSlots().size(), 0);
            cmd3.undo();
            QCOMPARE((int) rack->getSlots().size(), 1);
            cmd2.undo();
            cmd.undo();
            cmd.redo();
            cmd2.redo();
            cmd3.redo();

            QCOMPARE((int) rack->getSlots().size(), 0);






        }
};

QTEST_MAIN(RemoveSlotFromRackTest)
#include "RemoveSlotFromRackTest.moc"


