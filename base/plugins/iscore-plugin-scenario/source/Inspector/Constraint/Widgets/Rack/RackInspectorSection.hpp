#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <unordered_map>
class RackModel;
class SlotModel;
class AddSlotWidget;
class ConstraintInspectorWidget;
class SlotInspectorSection;

// Contains a single rack which can contain multiple slots and a Add Slot button.
class RackInspectorSection : public InspectorSectionWidget
{
    public:
        RackInspectorSection(
                const QString& name,
                const RackModel& model,
                ConstraintInspectorWidget* parent);

        void addSlotInspectorSection(const SlotModel&);
        void createSlot();

    public slots:
        void on_slotCreated(Id<SlotModel> slotId);
        void on_slotRemoved(Id<SlotModel> slotId);

    private:
        const RackModel& m_model;

        InspectorSectionWidget* m_slotSection {};
        AddSlotWidget* m_slotWidget {};
    public:
        ConstraintInspectorWidget* m_parent{};
    private:

        std::unordered_map<Id<SlotModel>, SlotInspectorSection*, id_hash<SlotModel>> m_slotsSectionWidgets;
        //std::vector<SlotInspectorSection*> m_slotsSectionWidgets;
};
