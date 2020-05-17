// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/AbstractScenarioLayerModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/AddRackToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Document/Event/EventData.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalViewModel.hpp>
#include <Scenario/Document/Interval/Rack/RackModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>

using namespace score;
using namespace Scenario::Command;

class HideRackInViewModelTest : public QObject
{
  Q_OBJECT
public:
private:
  void test()
  {
    CommandStack stack;
    // Maybe do a fake process list, with a fake process for unit tests.
    NamedObject* obj = new NamedObject{"obj", qApp};
    ProcessList* plist = new ProcessList{obj};
    plist->registerProcess(new ScenarioFactory);

    // Setup
    IntervalModel* interval
        = new IntervalModel{Id<IntervalModel>{0}, Id<IntervalViewModel>{0}, qApp};

    // Creation of a scenario with a interval
    auto cmd_proc = new AddProcessToInterval({{"IntervalModel", {}}}, "Scenario");
    stack.redoAndPush(cmd_proc);
    auto scenarioId = cmd_proc->m_createdProcessId;
    auto scenario = static_cast<Scenario::ProcessModel*>(interval->process(scenarioId));

    // Creation of a way to visualize what happens in the original interval
    auto cmd_rack = new AddRackToInterval(ObjectPath{{"IntervalModel", {}}});
    stack.redoAndPush(cmd_rack);
    auto rackId = cmd_rack->m_createdRackId;

    auto cmd_slot = new AddSlotToRack(ObjectPath{{"IntervalModel", {}}, {"RackModel", rackId}});
    auto slotId = cmd_slot->m_createdSlotId;
    stack.redoAndPush(cmd_slot);

    auto cmd_lm = new AddLayerModelToSlot(
        {{"IntervalModel", {}}, {"RackModel", rackId}, {"SlotModel", slotId}},
        {{"IntervalModel", {}}, {"ScenarioModel", scenarioId}});
    stack.redoAndPush(cmd_lm);

    auto viewmodel = interval->rackes().front()->getSlots().front()->layerModels().front();
    auto scenario_viewmodel = dynamic_cast<AbstractScenarioViewModel*>(viewmodel);
    // Put this in the tests for AbstractScenarioViewModel
    QVERIFY(scenario_viewmodel != nullptr);
    QCOMPARE(scenario_viewmodel->intervals().count(), 0);

    // Creation of an even and a interval inside the scenario
    EventData data{};
    // data.id = 0; unused here
    data.dDate.setMSecs(10);
    data.relativeY = 0.5;

    auto cmd_event = new CreateEvent(
        {
            {"ScenarioModel", {}},
        },
        data);
    stack.redoAndPush(cmd_event);

    // This will create a view model for this interval
    // in the previously-created Scenario View Model
    QCOMPARE(scenario_viewmodel->intervals().count(), 1);

    // Check that the interval view model is properly instantiated
    IntervalViewModel* interval_viewmodel = scenario_viewmodel->intervals().front();
    QCOMPARE(
        interval_viewmodel->model(), scenario->interval(cmd_event->m_cmd->m_createdIntervalId));
    QCOMPARE(interval_viewmodel->isRackShown(), false); // No rack can be
                                                        // shown since there
                                                        // isn't any in this
                                                        // interval

    auto cmd_rack2 = new AddRackToInterval(ObjectPath{
        {"IntervalModel", {}},
        {"ScenarioModel", scenarioId},
        {"IntervalModel", cmd_event->m_cmd->m_createdIntervalId}});
    stack.redoAndPush(cmd_rack2);

    QCOMPARE(
        interval_viewmodel->isRackShown(),
        false); // Now there is a rack but we do not show it
    auto rack2Id = cmd_rack2->m_createdRackId;

    // Show the rack
    auto cmd_showrack
        = new ShowRackInViewModel(score::IDocument::path(interval_viewmodel), rack2Id);
    stack.redoAndPush(cmd_showrack);
    QCOMPARE(interval_viewmodel->isRackShown(), true);
    QCOMPARE(interval_viewmodel->shownRack(), rack2Id);

    // And hide it
    auto cmd_hiderack = new HideRackInViewModel(score::IDocument::path(interval_viewmodel));
    stack.redoAndPush(cmd_hiderack);
    QCOMPARE(interval_viewmodel->isRackShown(), false);
    stack.undoQuiet();

    QCOMPARE(interval_viewmodel->isRackShown(), true);
    QCOMPARE(interval_viewmodel->shownRack(), rack2Id);
    stack.undoQuiet();

    QCOMPARE(interval_viewmodel->isRackShown(), false);

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

QTEST_MAIN(HideRackInViewModelTest)
#include "HideRackInViewModelTest.moc"
