#include <QtTest/QtTest>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>

using namespace iscore;
using namespace Scenario::Command;

class AddSlotToRackTest: public QObject
{
        Q_OBJECT

    private slots:
        void CreateSlotTest()
        {
            RackModel* rack  = new RackModel {Id<RackModel>{0}, qApp};

            QCOMPARE((int) rack->getSlots().size(), 0);
            AddSlotToRack cmd(
            ObjectPath { {"RackModel", {0}} });
            auto slotId = cmd.m_createdSlotId;

            cmd.redo();
            QCOMPARE((int) rack->getSlots().size(), 1);
            QCOMPARE(rack->slot(slotId)->parent(), rack);

            cmd.undo();
            QCOMPARE((int) rack->getSlots().size(), 0);

            cmd.redo();
            QCOMPARE((int) rack->getSlots().size(), 1);
            QCOMPARE(rack->slot(slotId)->parent(), rack);

            try
            {
                rack->slot(slotId);
            }
            catch(std::runtime_error& e)
            {
                QFAIL(e.what());
            }

            // Delete them else they stay in qApp !
            delete rack;
        }
};

QTEST_MAIN(AddSlotToRackTest)
#include "AddSlotToRackTest.moc"

