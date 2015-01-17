#include "DeckInspectorSection.hpp"

#include "AddProcessViewModelWidget.hpp"

#include "Inspector/Constraint/Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"

#include "ProcessInterface/ProcessViewModelInterface.hpp"

#include <QtWidgets>

using namespace Scenario::Command;

DeckInspectorSection::DeckInspectorSection(QString name,
										   DeckModel* deck,
										   BoxInspectorSection* parentBox):
	InspectorSectionWidget{name, parentBox},
	m_model{deck}
{
	m_pvmSection = new InspectorSectionWidget{"Process View Models", this};
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
	if(!pvm) return;

	auto widg = new QWidget{this};
	auto lay = new QHBoxLayout;
	widg->setLayout(lay);
	auto pb = new QPushButton{"Front"};
	connect(pb, &QPushButton::clicked,
			[=] () { m_model->selectForEdition((int)pvm->id()); });

	lay->addWidget(pb);
	lay->addWidget(new QLabel{QString{"ViewModel.%1"}.arg((int)pvm->id())});


	addContent(widg);
}


void DeckInspectorSection::on_processViewModelCreated(int pvmId)
{
	displayProcessViewModel(m_model->processViewModel(pvmId));
}

void DeckInspectorSection::on_processViewModelRemoved(int pvmId)
{
	// TODO
}
