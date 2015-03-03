#pragma once
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include <tools/SettableIdentifier.hpp>

class BoxModel;
class DeckModel;
class ConstraintInspectorWidget;
class ProcessViewModelInterface;
class AddProcessViewModelWidget;
class BoxInspectorSection;
class ProcessSharedModelInterface;

// Contains a single box which can contain multiple decks and a Add Deck button.
class DeckInspectorSection : public InspectorSectionWidget
{
    public:
        DeckInspectorSection(QString name,
                             DeckModel* deck,
                             BoxInspectorSection* parentBox);

        void displayProcessViewModel(ProcessViewModelInterface*);
        void createProcessViewModel(id_type<ProcessSharedModelInterface> sharedProcessId);

        DeckModel* model() const
        {
            return m_model;
        }

    public slots:
        void on_processViewModelCreated(id_type<ProcessViewModelInterface> id);
        void on_processViewModelRemoved(id_type<ProcessViewModelInterface> id);

    private:
        DeckModel* m_model {};

        ConstraintInspectorWidget* m_parent{};
        InspectorSectionWidget* m_pvmSection {};
        AddProcessViewModelWidget* m_addPvmWidget {};
        //std::vector<InspectorSectionWidget*> m_pvmsSectionWidgets;
};
