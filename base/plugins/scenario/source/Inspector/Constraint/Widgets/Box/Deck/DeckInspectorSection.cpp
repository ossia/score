#include "DeckInspectorSection.hpp"

#include "AddProcessViewModelWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"
#include "Inspector/Constraint/Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"

#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include "iscore/document/DocumentInterface.hpp"
#include <QtWidgets>

using namespace Scenario::Command;

DeckInspectorSection::DeckInspectorSection(QString name,
                                           DeckModel* deck,
                                           BoxInspectorSection* parentBox) :
    InspectorSectionWidget {name, parentBox},
    m_model {deck},
    m_parent{parentBox->m_parent}
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

void DeckInspectorSection::createProcessViewModel(id_type<ProcessSharedModelInterface> sharedProcessModelId)
{
    auto cmd = new AddProcessViewModelToDeck(
                   iscore::IDocument::path(m_model),
                   iscore::IDocument::path(m_model->parentConstraint()->process(sharedProcessModelId)));

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void DeckInspectorSection::displayProcessViewModel(ProcessViewModelInterface* pvm)
{
    if(!pvm)
    {
        return;
    }

    auto widg = new QWidget {this};
    auto lay = new QHBoxLayout;
    widg->setLayout(lay);
    auto pb = new QPushButton {"Front"};
    connect(pb, &QPushButton::clicked,
            [ = ]()
    {
        m_model->selectForEdition(pvm->id());
    });

    lay->addWidget(pb);
    lay->addWidget(new QLabel {QString{"ViewModel.%1"} .arg(*pvm->id().val()) });


    addContent(widg);
}


void DeckInspectorSection::on_processViewModelCreated(id_type<ProcessViewModelInterface> pvmId)
{
    displayProcessViewModel(m_model->processViewModel(pvmId));
}

void DeckInspectorSection::on_processViewModelRemoved(id_type<ProcessViewModelInterface> pvmId)
{
    // TODO
}
