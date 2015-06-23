#include "DeckInspectorSection.hpp"

#include "Inspector/Constraint/Widgets/Box/Deck/AddProcessViewModelWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"
#include "Inspector/Constraint/Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"

#include "ProcessInterface/ProcessViewModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"

#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"
#include "Commands/Constraint/Box/Deck/RemoveProcessViewModelFromDeck.hpp"
#include <QtWidgets>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

// TODO put in its own file. But it isn't a real command, only a presentation one.
class PutProcessViewModelToFront
{
    public:
        PutProcessViewModelToFront(ObjectPath&& deckPath,
                                   const id_type<ProcessViewModel>& pid):
            m_deckPath{std::move(deckPath)},
            m_pid{pid}
        {

        }

        void redo()
        {
            m_deckPath.find<DeckModel>().putToFront(m_pid);
        }

    private:
        ObjectPath m_deckPath;
        const id_type<ProcessViewModel>& m_pid;
};

using namespace Scenario::Command;

DeckInspectorSection::DeckInspectorSection(
        const QString& name,
        const DeckModel& deck,
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

    connect(&m_model,	&DeckModel::processViewModelCreated,
            this,		&DeckInspectorSection::on_processViewModelCreated);

    connect(&m_model,	&DeckModel::processViewModelRemoved,
            this,		&DeckInspectorSection::on_processViewModelRemoved);

    for(const auto& pvm : m_model.processViewModels())
    {
        displayProcessViewModel(*pvm);
    }

    m_addPvmWidget = new AddProcessViewModelWidget{this};
    lay->addWidget(m_pvmSection);
    lay->addWidget(m_addPvmWidget);


    // Delete button
    auto deleteButton = new QPushButton{"Delete"};
    connect(deleteButton, &QPushButton::pressed, this, [&] ()
    {
        auto cmd = new RemoveDeckFromBox{iscore::IDocument::path(m_model)};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });

    lay->addWidget(deleteButton);
}

void DeckInspectorSection::createProcessViewModel(
        const id_type<ProcessModel>& sharedProcessModelId)
{
    auto cmd = new AddProcessViewModelToDeck(
                   iscore::IDocument::path(m_model),
                   iscore::IDocument::path(m_model.parentConstraint().process(sharedProcessModelId)));

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void DeckInspectorSection::displayProcessViewModel(const ProcessViewModel& pvm)
{
    auto pvm_id = pvm.id();

    // Layout
    auto frame = new QFrame;
    auto lay = new QGridLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    frame->setLayout(lay);
    frame->setFrameShape(QFrame::StyledPanel);

    // PVM label
    lay->addWidget(new QLabel {QString{"ViewModel.%1"} .arg(*pvm_id.val()) }, 0, 0);

    // To front button
    auto pb = new QPushButton {tr("Front")};
    // TODO this should be a command! Maybe make a "view" command dispatcher
    // that doesn't forward to the command stack?
    connect(pb, &QPushButton::clicked,
            [=]() {
        PutProcessViewModelToFront cmd(iscore::IDocument::path(m_model), pvm_id);
        cmd.redo();
    });
    lay->addWidget(pb, 1, 0);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveProcessViewModelFromDeck{iscore::IDocument::path(m_model), pvm_id};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton, 1, 1);


    m_pvmSection->addContent(frame);
}


void DeckInspectorSection::on_processViewModelCreated(
        const id_type<ProcessViewModel>& pvmId)
{
    displayProcessViewModel(m_model.processViewModel(pvmId));
}

void DeckInspectorSection::on_processViewModelRemoved(
        const id_type<ProcessViewModel>& pvmId)
{
    qDebug() << Q_FUNC_INFO << "TODO";

    m_pvmSection->removeAll();
    for (auto& pvm : m_model.processViewModels())
    {
        if (pvm->id() != pvmId)
        {
            displayProcessViewModel(*pvm);
        }
    }
}


const DeckModel&DeckInspectorSection::model() const
{
    return m_model;
}
