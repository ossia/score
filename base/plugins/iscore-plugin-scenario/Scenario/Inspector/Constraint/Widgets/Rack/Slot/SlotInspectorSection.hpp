#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <QString>
#include <iscore/tools/SettableIdentifier.hpp>
#include <nano_signal_slot.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }


namespace Scenario
{
class AddLayerModelWidget;
class RackInspectorSection;
class SlotModel;
class ConstraintInspectorWidget;
// Contains a single rack which can contain multiple slots and a Add Slot button.
class SlotInspectorSection final : public Inspector::InspectorSectionWidget, public Nano::Observer
{
    public:
        SlotInspectorSection(
                const QString& name,
                const SlotModel& slot,
                RackInspectorSection* parentRack);

        void displayLayerModel(const Process::LayerModel&);
        void createLayerModel(
                const Id<Process::ProcessModel>& sharedProcessId);

        const SlotModel& model() const;

    private:
        void ask_changeName(QString newName);

        void on_layerModelCreated(const Process::LayerModel&);
        void on_layerModelRemoved(const Process::LayerModel&);

        const SlotModel& m_model;

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_lmSection {};
        AddLayerModelWidget* m_addLmWidget {};
        //std::vector<InspectorSectionWidget*> m_lmsSectionWidgets;
};
}
