#include <QtTest/QtTest>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Rack/RackModel.hpp>
#include <Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <ProcessInterface/LayerModel.hpp>
#include <ProcessInterface/ProcessModel.hpp>

#include "Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddRackToConstraint.hpp"
#include "Commands/Constraint/Rack/AddSlotToRack.hpp"
#include <Process/ScenarioFactory.hpp>
#include "ProcessInterface/ProcessList.hpp"

#include <core/command/CommandStack.hpp>

using namespace iscore;
using namespace Scenario::Command;

class AddLayerModelToSlotTest: public QObject
{
        Q_OBJECT

    private slots:
        void CreateViewModelTest()
        {
            CommandStack stack;
            // Maybe do a fake process list, with a fake process for unit tests.
            NamedObject* obj = new NamedObject {"obj", qApp};
            ProcessList* plist = new ProcessList {obj};
            plist->registerProcess(new ScenarioFactory);

            // Setup
            ConstraintModel* constraint  = new ConstraintModel {Id<ConstraintModel>{0}, Id<ConstraintViewModel>{0}, qApp};

            auto cmd_proc = new AddProcessToConstraint(
            {
                {"ConstraintModel", {0}}
            }, "Scenario");
            stack.redoAndPush(cmd_proc);
            auto procId = cmd_proc->m_createdProcessId;

            auto cmd_rack = new AddRackToConstraint(
            ObjectPath { {"ConstraintModel", {0}} });
            stack.redoAndPush(cmd_rack);
            auto rackId = cmd_rack->m_createdRackId;

            auto cmd_slot = new AddSlotToRack(
                ObjectPath
            {
                {"ConstraintModel", {0}},
                {"RackModel", rackId}
            });
            auto slotId = cmd_slot->m_createdSlotId;
            stack.redoAndPush(cmd_slot);

            auto cmd_lm = new AddLayerModelToSlot(
            {
                {"ConstraintModel", {0}},
                {"RackModel", rackId},
                {"SlotModel", slotId}
            },
            {
                {"ConstraintModel", {0}},
                {"ScenarioModel", procId}
            });
            stack.redoAndPush(cmd_lm);

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

QTEST_MAIN(AddLayerModelToSlotTest)
#include "AddLayerModelToSlotTest.moc"

