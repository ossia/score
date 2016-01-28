#include <QtTest/QtTest>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventData.hpp>

#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>

#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Process/AbstractScenarioLayerModel.hpp>

#include <Process/ProcessList.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/command/CommandStack.hpp>
using namespace iscore;
using namespace Scenario::Command;

class HideRackInViewModelTest: public QObject
{
        Q_OBJECT
    public:

    private slots:
        void test()
        {
            CommandStack stack;
            // Maybe do a fake process list, with a fake process for unit tests.
            NamedObject* obj = new NamedObject {"obj", qApp};
            ProcessList* plist = new ProcessList {obj};
            plist->registerProcess(new ScenarioFactory);

            // Setup
            ConstraintModel* constraint  = new ConstraintModel {Id<ConstraintModel>{0},
                                                                Id<ConstraintViewModel>{0}, qApp
                                                               };

            // Creation of a scenario with a constraint
            auto cmd_proc = new AddProcessToConstraint(
            {
                {"ConstraintModel", {}}
            }, "Scenario");
            stack.redoAndPush(cmd_proc);
            auto scenarioId = cmd_proc->m_createdProcessId;
            auto scenario = static_cast<Scenario::ScenarioModel*>(constraint->process(scenarioId));


            // Creation of a way to visualize what happens in the original constraint
            auto cmd_rack = new AddRackToConstraint(
            ObjectPath { {"ConstraintModel", {}} });
            stack.redoAndPush(cmd_rack);
            auto rackId = cmd_rack->m_createdRackId;

            auto cmd_slot = new AddSlotToRack(
                ObjectPath
            {
                {"ConstraintModel", {}},
                {"RackModel", rackId}
            });
            auto slotId = cmd_slot->m_createdSlotId;
            stack.redoAndPush(cmd_slot);

            auto cmd_lm = new AddLayerModelToSlot(
            {
                {"ConstraintModel", {}},
                {"RackModel", rackId},
                {"SlotModel", slotId}
            },
            {
                {"ConstraintModel", {}},
                {"ScenarioModel", scenarioId}
            });
            stack.redoAndPush(cmd_lm);

            auto viewmodel = constraint->rackes().front()->getSlots().front()->layerModels().front();
            auto scenario_viewmodel = dynamic_cast<AbstractScenarioViewModel*>(viewmodel);
            // Put this in the tests for AbstractScenarioViewModel
            QVERIFY(scenario_viewmodel != nullptr);
            QCOMPARE(scenario_viewmodel->constraints().count(), 0);


            // Creation of an even and a constraint inside the scenario
            EventData data {};
            // data.id = 0; unused here
            data.dDate.setMSecs(10);
            data.relativeY = 0.5;

            auto cmd_event = new CreateEvent(
            {
                {"ScenarioModel", {}},
            }, data);
            stack.redoAndPush(cmd_event);

            // This will create a view model for this constraint
            // in the previously-created Scenario View Model
            QCOMPARE(scenario_viewmodel->constraints().count(), 1);

            // Check that the constraint view model is properly instantiated
            ConstraintViewModel* constraint_viewmodel = scenario_viewmodel->constraints().front();
            QCOMPARE(constraint_viewmodel->model(), scenario->constraint(cmd_event->m_cmd->m_createdConstraintId));
            QCOMPARE(constraint_viewmodel->isRackShown(), false);  // No rack can be shown since there isn't any in this constraint

            auto cmd_rack2 = new AddRackToConstraint(
                ObjectPath
            {
                {"ConstraintModel", {}},
                {"ScenarioModel", scenarioId},
                {"ConstraintModel", cmd_event->m_cmd->m_createdConstraintId}
            });
            stack.redoAndPush(cmd_rack2);

            QCOMPARE(constraint_viewmodel->isRackShown(), false);  // Now there is a rack but we do not show it
            auto rack2Id = cmd_rack2->m_createdRackId;


            // Show the rack
            auto cmd_showrack = new ShowRackInViewModel(iscore::IDocument::path(constraint_viewmodel), rack2Id);
            stack.redoAndPush(cmd_showrack);
            QCOMPARE(constraint_viewmodel->isRackShown(), true);
            QCOMPARE(constraint_viewmodel->shownRack(), rack2Id);

            // And hide it
            auto cmd_hiderack = new HideRackInViewModel(iscore::IDocument::path(constraint_viewmodel));
            stack.redoAndPush(cmd_hiderack);
            QCOMPARE(constraint_viewmodel->isRackShown(), false);
            stack.undoQuiet();

            QCOMPARE(constraint_viewmodel->isRackShown(), true);
            QCOMPARE(constraint_viewmodel->shownRack(), rack2Id);
            stack.undoQuiet();

            QCOMPARE(constraint_viewmodel->isRackShown(), false);


            for(int i = 4; i -- > 0;)
            {
                while(stack.canUndo())
                {
                    stack.undoQuiet();
                }

                while(stack.canRedo())
                {
                    stack.redoQuiet();
                }
            }

            delete constraint;
        }
};

QTEST_MAIN(HideRackInViewModelTest)
#include "HideRackInViewModelTest.moc"


