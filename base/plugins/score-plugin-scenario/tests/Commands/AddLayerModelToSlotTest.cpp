// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>

#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/AddRackToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Interval/AddLayerModelToSlot.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>

#include <core/command/CommandStack.hpp>

using namespace score;
using namespace Scenario::Command;

class AddLayerModelToSlotTest : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void CreateViewModelTest()
  {
    CommandStack stack;
    // Maybe do a fake process list, with a fake process for unit tests.
    NamedObject* obj = new NamedObject{"obj", qApp};
    ProcessList* plist = new ProcessList{obj};
    plist->registerProcess(new ScenarioFactory);

    // Setup
    IntervalModel* interval = new IntervalModel{
        Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, qApp};

    auto cmd_proc
        = new AddProcessToInterval({{"IntervalModel", {0}}}, "Scenario");
    stack.redoAndPush(cmd_proc);
    auto procId = cmd_proc->m_createdProcessId;

    auto cmd_rack
        = new AddRackToInterval(ObjectPath{{"IntervalModel", {0}}});
    stack.redoAndPush(cmd_rack);
    auto rackId = cmd_rack->m_createdRackId;

    auto cmd_slot = new AddSlotToRack(
        ObjectPath{{"IntervalModel", {0}}, {"RackModel", rackId}});
    auto slotId = cmd_slot->m_createdSlotId;
    stack.redoAndPush(cmd_slot);

    auto cmd_lm = new AddLayerModelToSlot(
        {{"IntervalModel", {0}},
         {"RackModel", rackId},
         {"SlotModel", slotId}},
        {{"IntervalModel", {0}}, {"ScenarioModel", procId}});
    stack.redoAndPush(cmd_lm);

    for (int i = 4; i-- > 0;)
    {
      while (stack.canUndo())
      {
        stack.undoQuiet();
      }

      while (stack.canRedo())
      {
        stack.redoQuiet();
      }
    }

    delete interval;
  }
};

QTEST_MAIN(AddLayerModelToSlotTest)
#include "AddLayerModelToSlotTest.moc"
