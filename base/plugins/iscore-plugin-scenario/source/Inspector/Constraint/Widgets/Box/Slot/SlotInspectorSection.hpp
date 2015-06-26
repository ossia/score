#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class BoxModel;
class SlotModel;
class ConstraintInspectorWidget;
class ProcessViewModel;
class AddProcessViewModelWidget;
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

        void displayProcessViewModel(const ProcessViewModel&);
        void createProcessViewModel(
                const id_type<ProcessModel>& sharedProcessId);

        const SlotModel& model() const;

    public slots:
        void on_processViewModelCreated(const id_type<ProcessViewModel>& id);
        void on_processViewModelRemoved(const id_type<ProcessViewModel>& id);

    private:
        const SlotModel& m_model;

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_pvmSection {};
        AddProcessViewModelWidget* m_addPvmWidget {};
        //std::vector<InspectorSectionWidget*> m_pvmsSectionWidgets;
};
