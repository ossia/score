#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <nano_signal_slot.hpp>
#include <QString>
#include <unordered_map>

#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>

namespace Scenario
{
class AddSlotWidget;
class RackModel;
class SlotInspectorSection;
class SlotModel;

// Contains a single rack which can contain multiple slots and a Add Slot button.
class RackInspectorSection final :
        public Inspector::InspectorSectionWidget,
        public Nano::Observer
{
    public:
        RackInspectorSection(
                const QString& name,
                const RackModel& model,
                const ConstraintInspectorWidget& parentCstr,
                QWidget* parent);

        void addSlotInspectorSection(const SlotModel&);
        void createSlot();

        const ConstraintInspectorWidget& constraintInspector() const
        { return m_parent; }

    private:
        const ConstraintInspectorWidget& m_parent;
        const RackModel& m_model;

        void ask_changeName(QString newName);

        void on_slotCreated(const SlotModel&);
        void on_slotRemoved(const SlotModel&);

        InspectorSectionWidget* m_slotSection {};
        AddSlotWidget* m_slotWidget {};

        std::unordered_map<Id<SlotModel>, SlotInspectorSection*> slotmodelsSectionWidgets;
};
}
