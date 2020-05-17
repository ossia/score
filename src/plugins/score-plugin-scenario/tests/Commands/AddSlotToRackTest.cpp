// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

using namespace score;
using namespace Scenario::Command;

class AddSlotToRackTest : public QObject
{
  Q_OBJECT

private:
  void CreateSlotTest()
  {
    RackModel* rack = new RackModel{Id<RackModel>{0}, qApp};

    QCOMPARE((int)rack->getSlots().size(), 0);
    AddSlotToRack cmd(ObjectPath{{"RackModel", {0}}});
    auto slotId = cmd.m_createdSlotId;

    cmd.redo(ctx);
    QCOMPARE((int)rack->getSlots().size(), 1);
    QCOMPARE(rack->slot(slotId)->parent(), rack);

    cmd.undo(ctx);
    QCOMPARE((int)rack->getSlots().size(), 0);

    cmd.redo(ctx);
    QCOMPARE((int)rack->getSlots().size(), 1);
    QCOMPARE(rack->slot(slotId)->parent(), rack);

    try
    {
      rack->slot(slotId);
    }
    catch (std::runtime_error& e)
    {
      QFAIL(e.what());
    }

    // Delete them else they stay in qApp !
    delete rack;
  }
};

QTEST_MAIN(AddSlotToRackTest)
#include "AddSlotToRackTest.moc"
