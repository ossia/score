#include "SlotInspectorSection.hpp"

#include "Inspector/Constraint/Widgets/Box/Slot/AddProcessViewModelWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"
#include "Inspector/Constraint/Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

#include "Commands/Constraint/Box/Slot/AddProcessViewModelToSlot.hpp"

#include "ProcessInterface/ProcessViewModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"

#include "Commands/Constraint/Box/RemoveSlotFromBox.hpp"
#include "Commands/Constraint/Box/Slot/RemoveProcessViewModelFromSlot.hpp"

#include "ViewCommands/PutProcessViewModelToFront.hpp"
#include <QtWidgets>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

using namespace Scenario::Command;

SlotInspectorSection::SlotInspectorSection(
        const QString& name,
        const SlotModel& slot,
        BoxInspectorSection* parentBox) :
    InspectorSectionWidget {name, parentBox},
    m_model {slot},
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

    connect(&m_model,	&SlotModel::processViewModelCreated,
            this,		&SlotInspectorSection::on_processViewModelCreated);

    connect(&m_model,	&SlotModel::processViewModelRemoved,
            this,		&SlotInspectorSection::on_processViewModelRemoved);

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
        auto cmd = new RemoveSlotFromBox{iscore::IDocument::path(m_model)};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });

    lay->addWidget(deleteButton);
}

void SlotInspectorSection::createProcessViewModel(
        const id_type<ProcessModel>& sharedProcessModelId)
{
    auto cmd = new AddProcessViewModelToSlot(
                   iscore::IDocument::path(m_model),
                   iscore::IDocument::path(m_model.parentConstraint().process(sharedProcessModelId)));

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void SlotInspectorSection::displayProcessViewModel(const ProcessViewModel& pvm)
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
        auto cmd = new RemoveProcessViewModelFromSlot{iscore::IDocument::path(m_model), pvm_id};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton, 1, 1);


    m_pvmSection->addContent(frame);
}


void SlotInspectorSection::on_processViewModelCreated(
        const id_type<ProcessViewModel>& pvmId)
{
    displayProcessViewModel(m_model.processViewModel(pvmId));
}

void SlotInspectorSection::on_processViewModelRemoved(
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


const SlotModel&SlotInspectorSection::model() const
{
    return m_model;
}
