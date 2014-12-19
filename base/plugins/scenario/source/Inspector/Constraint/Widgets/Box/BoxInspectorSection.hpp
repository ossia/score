#pragma once
#include <InspectorInterface/InspectorSectionWidget.hpp>

class BoxModel;
class StoreyModel;
class AddDeckWidget;
class ConstraintInspectorWidget;
class DeckInspectorSection;

// Contains a single box which can contain multiple decks and a Add Deck button.
class BoxInspectorSection : public InspectorSectionWidget
{
	public:
		BoxInspectorSection(QString name, BoxModel* model, ConstraintInspectorWidget* parent);

		void setModel(BoxModel*);

		void displayDeck(StoreyModel*);
		void createDeck();

	public slots:
		void on_deckCreated(int deckId);
		void on_deckRemoved(int deckId);

	private:
		BoxModel* m_model{};

		InspectorSectionWidget* m_deckSection{};
		AddDeckWidget* m_deckWidget{};
		std::vector<DeckInspectorSection*> m_decksSectionWidgets;
};
