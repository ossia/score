// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <Scenario/Commands/Interval/AddRackToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Interval/Rack/RemoveSlotFromRack.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

using namespace score;
using namespace Scenario::Command;

class RemoveSlotFromRackTest : public QObject
{
  Q_OBJECT
public:
private Q_SLOTS:
  void test()
  {
    IntervalModel* interval = new IntervalModel{
        Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, qApp};

    AddRackToInterval cmd{ObjectPath{{"IntervalModel", {}}}};

    cmd.redo(ctx);
    auto rack = interval->rack(cmd.m_createdRackId);

    AddSlotToRack cmd2{
        ObjectPath{{"IntervalModel", {}}, {"RackModel", rack->id()}}};

    auto slotId = cmd2.m_createdSlotId;
    cmd2.redo(ctx);

    RemoveSlotFromRack cmd3{
        ObjectPath{{"IntervalModel", {}}, {"RackModel", rack->id()}},
        slotId};

    QCOMPARE((int)rack->getSlots().size(), 1);
    cmd3.redo(ctx);
    QCOMPARE((int)rack->getSlots().size(), 0);
    cmd3.undo(ctx);
    QCOMPARE((int)rack->getSlots().size(), 1);
    cmd2.undo(ctx);
    cmd.undo(ctx);
    cmd.redo(ctx);
    cmd2.redo(ctx);
    cmd3.redo(ctx);

    QCOMPARE((int)rack->getSlots().size(), 0);
  }
};

QTEST_MAIN(RemoveSlotFromRackTest)
#include "RemoveSlotFromRackTest.moc"
