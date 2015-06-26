#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class BoxModel;
class SlotModel;
class ConstraintInspectorWidget;
class LayerModel;
class AddLayerModelWidget;
class BoxInspectorSection;
class ProcessModel;

// Contains a single box which can contain multiple slots and a Add Slot button.
class SlotInspectorSection : public InspectorSectionWidget
{
    public:
        SlotInspectorSection(
                const QString& name,
                const SlotModel& slot,
                BoxInspectorSection* parentBox);

        void displayLayerModel(const LayerModel&);
        void createLayerModel(
                const id_type<ProcessModel>& sharedProcessId);

        const SlotModel& model() const;

    public slots:
        void on_layerModelCreated(const id_type<LayerModel>& id);
        void on_layerModelRemoved(const id_type<LayerModel>& id);

    private:
        const SlotModel& m_model;

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_pvmSection {};
        AddLayerModelWidget* m_addPvmWidget {};
        //std::vector<InspectorSectionWidget*> m_pvmsSectionWidgets;
};
