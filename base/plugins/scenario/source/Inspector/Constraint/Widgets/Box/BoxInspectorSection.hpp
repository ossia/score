#pragma once
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include <public_interface/tools/SettableIdentifier.hpp>
#include <unordered_map>
class BoxModel;
class DeckModel;
class AddDeckWidget;
class ConstraintInspectorWidget;
class DeckInspectorSection;

// Contains a single box which can contain multiple decks and a Add Deck button.
class BoxInspectorSection : public InspectorSectionWidget
{
    public:
        BoxInspectorSection(QString name, BoxModel* model, ConstraintInspectorWidget* parent);

        void addDeckInspectorSection(DeckModel*);
        void createDeck();

    public slots:
        void on_deckCreated(id_type<DeckModel> deckId);
        void on_deckRemoved(id_type<DeckModel> deckId);

    private:
        BoxModel* m_model {};

        InspectorSectionWidget* m_deckSection {};
        AddDeckWidget* m_deckWidget {};
    public:
        ConstraintInspectorWidget* m_parent{};
    private:

        std::unordered_map<id_type<DeckModel>, DeckInspectorSection*, id_hash<DeckModel>> m_decksSectionWidgets;
        //std::vector<DeckInspectorSection*> m_decksSectionWidgets;
};
