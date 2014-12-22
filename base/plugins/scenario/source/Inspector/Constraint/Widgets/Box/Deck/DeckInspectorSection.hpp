#pragma once
#include <InspectorInterface/InspectorSectionWidget.hpp>

class BoxModel;
class DeckModel;
class ConstraintInspectorWidget;
class ProcessViewModelInterface;
class AddProcessViewModelWidget;
class BoxInspectorSection;

// Contains a single box which can contain multiple decks and a Add Deck button.
class DeckInspectorSection : public InspectorSectionWidget
{
	public:
		DeckInspectorSection(QString name,
							 DeckModel* deck,
							 BoxInspectorSection* parentBox);

		void displayProcessViewModel(ProcessViewModelInterface*);
		void createProcessViewModel(int sharedProcessId);

		DeckModel* model() const
		{ return m_model; }

	public slots:
		void on_processViewModelCreated(int id);
		void on_processViewModelRemoved(int id);

	private:
		DeckModel* m_model{};

		InspectorSectionWidget* m_pvmSection{};
		AddProcessViewModelWidget* m_addPvmWidget{};
		//std::vector<InspectorSectionWidget*> m_pvmsSectionWidgets;
};
