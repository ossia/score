#include <QtTest/QtTest>

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/Slot/SlotModel.hpp>
#include <ProcessInterface/LayerModel.hpp>
#include <ProcessInterface/ProcessModel.hpp>

#include "Commands/Constraint/Box/Slot/AddLayerModelToSlot.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddSlotToBox.hpp"
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
            ConstraintModel* constraint  = new ConstraintModel {id_type<ConstraintModel>{0}, id_type<AbstractConstraintViewModel>{0}, qApp};

            auto cmd_proc = new AddProcessToConstraint(
            {
                {"ConstraintModel", {0}}
            }, "Scenario");
            stack.redoAndPush(cmd_proc);
            auto procId = cmd_proc->m_createdProcessId;

            auto cmd_box = new AddBoxToConstraint(
            ObjectPath { {"ConstraintModel", {0}} });
            stack.redoAndPush(cmd_box);
            auto boxId = cmd_box->m_createdBoxId;

            auto cmd_slot = new AddSlotToBox(
                ObjectPath
            {
                {"ConstraintModel", {0}},
                {"BoxModel", boxId}
            });
            auto slotId = cmd_slot->m_createdSlotId;
            stack.redoAndPush(cmd_slot);

            auto cmd_pvm = new AddLayerModelToSlot(
            {
                {"ConstraintModel", {0}},
                {"BoxModel", boxId},
                {"SlotModel", slotId}
            },
            {
                {"ConstraintModel", {0}},
                {"ScenarioModel", procId}
            });
            stack.redoAndPush(cmd_pvm);

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

