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

#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"
#include "Commands/Constraint/Box/Deck/RemoveProcessViewModelFromDeck.hpp"
#include <QtWidgets>

using namespace Scenario::Command;

DeckInspectorSection::DeckInspectorSection(QString name,
                                           DeckModel* deck,
                                           BoxInspectorSection* parentBox) :
    InspectorSectionWidget {name, parentBox},
    m_model {deck},
    m_parent{parentBox->m_parent}
{
    auto framewidg = new QFrame;
    auto lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    framewidg->setLayout(lay);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    // View model list
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
    lay->addWidget(m_pvmSection);
    lay->addWidget(m_addPvmWidget);


    // Delete button
    auto deleteButton = new QPushButton{"Delete"};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveDeckFromBox{iscore::IDocument::path(deck)};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });

    lay->addWidget(deleteButton);
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

    // Layout
    auto frame = new QFrame;
    auto lay = new QGridLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    frame->setLayout(lay);
    frame->setFrameShape(QFrame::StyledPanel);

    // PVM label
    lay->addWidget(new QLabel {QString{"ViewModel.%1"} .arg(*pvm->id().val()) }, 0, 0);

    // To front button
    auto pb = new QPushButton {tr("Front")};
    connect(pb, &QPushButton::clicked,
            [=]() { m_model->selectForEdition(pvm->id()); });
    lay->addWidget(pb, 1, 0);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveProcessViewModelFromDeck{iscore::IDocument::path(m_model), pvm->id()};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton, 1, 1);


    m_pvmSection->addContent(frame);
}


void DeckInspectorSection::on_processViewModelCreated(id_type<ProcessViewModelInterface> pvmId)
{
    displayProcessViewModel(m_model->processViewModel(pvmId));
}

void DeckInspectorSection::on_processViewModelRemoved(id_type<ProcessViewModelInterface> pvmId)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}
