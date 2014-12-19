#pragma once
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include <QMap>
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

		void addDeckInspectorSection(StoreyModel*);
		void createDeck();

	public slots:
		void on_deckCreated(int deckId);
		void on_deckRemoved(int deckId);

	private:
		BoxModel* m_model{};

		InspectorSectionWidget* m_deckSection{};
		AddDeckWidget* m_deckWidget{};

		QMap<int, DeckInspectorSection*> m_decksSectionWidgets;
		//std::vector<DeckInspectorSection*> m_decksSectionWidgets;
};
