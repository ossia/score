#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <QString>
#include <nano_signal_slot.hpp>

class AddLayerModelWidget;
class ConstraintInspectorWidget;
class LayerModel;
class Process;
class RackInspectorSection;
class SlotModel;
#include <iscore/tools/SettableIdentifier.hpp>

// Contains a single rack which can contain multiple slots and a Add Slot button.
class SlotInspectorSection final : public InspectorSectionWidget, public Nano::Observer
{
    public:
        SlotInspectorSection(
                const QString& name,
                const SlotModel& slot,
                RackInspectorSection* parentRack);

        void displayLayerModel(const LayerModel&);
        void createLayerModel(
                const Id<Process>& sharedProcessId);

        const SlotModel& model() const;

    private slots:
        void ask_changeName(QString newName);

    private:
        void on_layerModelCreated(const LayerModel&);
        void on_layerModelRemoved(const LayerModel&);

        const SlotModel& m_model;

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_lmSection {};
        AddLayerModelWidget* m_addLmWidget {};
        //std::vector<InspectorSectionWidget*> m_lmsSectionWidgets;
};
