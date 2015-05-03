#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class BoxModel;
class DeckModel;
class ConstraintInspectorWidget;
class ProcessViewModelInterface;
class AddProcessViewModelWidget;
class BoxInspectorSection;
class ProcessModel;

// Contains a single box which can contain multiple decks and a Add Deck button.
class DeckInspectorSection : public InspectorSectionWidget
{
    public:
        DeckInspectorSection(
                const QString& name,
                const DeckModel& deck,
                BoxInspectorSection* parentBox);

        void displayProcessViewModel(const ProcessViewModelInterface&);
        void createProcessViewModel(
                const id_type<ProcessModel>& sharedProcessId);

        const DeckModel& model() const;

    public slots:
        void on_processViewModelCreated(const id_type<ProcessViewModelInterface>& id);
        void on_processViewModelRemoved(const id_type<ProcessViewModelInterface>& id);

    private:
        const DeckModel& m_model;

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_pvmSection {};
        AddProcessViewModelWidget* m_addPvmWidget {};
        //std::vector<InspectorSectionWidget*> m_pvmsSectionWidgets;
};
