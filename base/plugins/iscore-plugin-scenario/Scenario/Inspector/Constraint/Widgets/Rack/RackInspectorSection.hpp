#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <nano_signal_slot.hpp>
#include <QString>
#include <unordered_map>

class AddSlotWidget;
class ConstraintInspectorWidget;
class RackModel;
class SlotInspectorSection;
class SlotModel;

// Contains a single rack which can contain multiple slots and a Add Slot button.
class RackInspectorSection final : public InspectorSectionWidget, public Nano::Observer
{
    public:
        RackInspectorSection(
                const QString& name,
                const RackModel& model,
                ConstraintInspectorWidget* parent);

        void addSlotInspectorSection(const SlotModel&);
        void createSlot();

        auto constraintInspector() const
        { return m_parent; }

    private:
        void ask_changeName(QString newName);

        ConstraintInspectorWidget* m_parent{};

        void on_slotCreated(const SlotModel&);
        void on_slotRemoved(const SlotModel&);
        const RackModel& m_model;

        InspectorSectionWidget* m_slotSection {};
        AddSlotWidget* m_slotWidget {};

        std::unordered_map<Id<SlotModel>, SlotInspectorSection*, id_hash<SlotModel>> slotmodelsSectionWidgets;
};
