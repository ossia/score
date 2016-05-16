#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <QString>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/widgets/CommandSpinBox.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>

#include <nano_signal_slot.hpp>
namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }

class QGridLayout;
class QSpinBox;
namespace Scenario
{
class AddLayerModelWidget;
class RackInspectorSection;
class ConstraintInspectorWidget;
// Contains a single rack which can contain multiple slots and a Add Slot button.
class SlotInspectorSection final :
        public Inspector::InspectorSectionWidget,
        public Nano::Observer
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

        const ConstraintInspectorWidget& m_parent;
        InspectorSectionWidget* m_lmSection {};
        AddLayerModelWidget* m_addLmWidget {};
        QGridLayout* m_lmGridLayout{};

        iscore::CommandSpinbox<SlotModelHeightParameter, Command::ResizeSlotVertically, QSpinBox> m_sb;
};
}
