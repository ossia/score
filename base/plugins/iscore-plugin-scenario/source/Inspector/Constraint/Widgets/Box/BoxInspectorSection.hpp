#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <unordered_map>
class BoxModel;
class SlotModel;
class AddSlotWidget;
class ConstraintInspectorWidget;
class SlotInspectorSection;

// Contains a single box which can contain multiple slots and a Add Slot button.
class BoxInspectorSection : public InspectorSectionWidget
{
    public:
        BoxInspectorSection(QString name, BoxModel* model, ConstraintInspectorWidget* parent);

        void addSlotInspectorSection(SlotModel*);
        void createSlot();

    public slots:
        void on_slotCreated(id_type<SlotModel> slotId);
        void on_slotRemoved(id_type<SlotModel> slotId);

    private:
        BoxModel* m_model {};

        InspectorSectionWidget* m_slotSection {};
        AddSlotWidget* m_slotWidget {};
    public:
        ConstraintInspectorWidget* m_parent{};
    private:

        std::unordered_map<id_type<SlotModel>, SlotInspectorSection*, id_hash<SlotModel>> m_slotsSectionWidgets;
        //std::vector<SlotInspectorSection*> m_slotsSectionWidgets;
};
