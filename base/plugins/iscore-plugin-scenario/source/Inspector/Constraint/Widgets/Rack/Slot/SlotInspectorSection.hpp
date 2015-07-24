#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class RackModel;
class SlotModel;
class ConstraintInspectorWidget;
class LayerModel;
class AddLayerModelWidget;
class RackInspectorSection;
class Process;

// Contains a single rack which can contain multiple slots and a Add Slot button.
class SlotInspectorSection : public InspectorSectionWidget
{
    public:
        SlotInspectorSection(
                const QString& name,
                const SlotModel& slot,
                RackInspectorSection* parentRack);

        void displayLayerModel(const LayerModel&);
        void createLayerModel(
                const id_type<Process>& sharedProcessId);

        const SlotModel& model() const;

    public slots:
        void on_layerModelCreated(const id_type<LayerModel>& id);
        void on_layerModelRemoved(const id_type<LayerModel>& id);

    private:
        const SlotModel& m_model;

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_lmSection {};
        AddLayerModelWidget* m_addLmWidget {};
        //std::vector<InspectorSectionWidget*> m_lmsSectionWidgets;
};
