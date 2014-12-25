#include "DeckInspectorSection.hpp"

#include "AddProcessViewModelWidget.hpp"

#include "Inspector/Constraint/Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"

#include <QtWidgets/QVBoxLayout>

using namespace Scenario::Command;

DeckInspectorSection::DeckInspectorSection(QString name,
										   DeckModel* deck,
										   BoxInspectorSection* parentBox):
	InspectorSectionWidget{name, parentBox},
	m_model{deck}
{
	m_pvmSection = new InspectorSectionWidget{"Process View Models", this};  // TODO Make a custom widget.
	m_pvmSection->setObjectName("ProcessViewModels");

	connect(m_model,	&DeckModel::processViewModelCreated,
			this,		&DeckInspectorSection::on_processViewModelCreated);

	connect(m_model,	&DeckModel::processViewModelRemoved,
			this,		&DeckInspectorSection::on_processViewModelRemoved);

	for(auto& pvm : m_model->processViewModels())
	{
		displayProcessViewModel(pvm);
	}

	m_addPvmWidget = new AddProcessViewModelWidget{this};
	addContent(m_pvmSection);
	addContent(m_addPvmWidget);
}

void DeckInspectorSection::createProcessViewModel(int sharedProcessModelId)
{
	auto cmd = new AddProcessViewModelToDeck(
						ObjectPath::pathFromObject(
							"BaseConstraintModel",
							m_model),
						sharedProcessModelId);

	emit submitCommand(cmd);
}

void DeckInspectorSection::displayProcessViewModel(ProcessViewModelInterface* pvm)
{
	//	InspectorSectionWidget* newDeck = new InspectorSectionWidget{QString{"Deck.%1"}.arg(pvm->id())};
	//	m_deckSection->addContent(newDeck);
}


void DeckInspectorSection::on_processViewModelCreated(int deckId)
{
	//displayDeck(m_model->deck(deckId));
}

void DeckInspectorSection::on_processViewModelRemoved(int deckId)
{
	// TODO
}
