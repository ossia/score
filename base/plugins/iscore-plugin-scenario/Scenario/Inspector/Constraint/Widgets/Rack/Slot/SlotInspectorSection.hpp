#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <QString>

class AddLayerModelWidget;
class ConstraintInspectorWidget;
class LayerModel;
class Process;
class RackInspectorSection;
class SlotModel;
template <typename tag, typename impl> class id_base_t;

// Contains a single rack which can contain multiple slots and a Add Slot button.
class SlotInspectorSection final : public InspectorSectionWidget
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
