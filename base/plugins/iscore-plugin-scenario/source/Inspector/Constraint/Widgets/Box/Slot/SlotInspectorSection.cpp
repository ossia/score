#include "SlotInspectorSection.hpp"

#include "Inspector/Constraint/Widgets/Box/Slot/AddLayerModelWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"
#include "Inspector/Constraint/Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

#include "Commands/Constraint/Box/Slot/AddLayerModelToSlot.hpp"

#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"

#include "Commands/Constraint/Box/RemoveSlotFromBox.hpp"
#include "Commands/Constraint/Box/Slot/RemoveLayerModelFromSlot.hpp"

#include "ViewCommands/PutLayerModelToFront.hpp"
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
    m_pvmSection->setObjectName("LayerModels");

    connect(&m_model,	&SlotModel::layerModelCreated,
            this,		&SlotInspectorSection::on_layerModelCreated);

    connect(&m_model,	&SlotModel::layerModelRemoved,
            this,		&SlotInspectorSection::on_layerModelRemoved);

    for(const auto& pvm : m_model.layerModels())
    {
        displayLayerModel(*pvm);
    }

    m_addPvmWidget = new AddLayerModelWidget{this};
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

void SlotInspectorSection::createLayerModel(
        const id_type<ProcessModel>& sharedProcessModelId)
{
    auto cmd = new AddLayerModelToSlot(
                   iscore::IDocument::path(m_model),
                   iscore::IDocument::path(m_model.parentConstraint().process(sharedProcessModelId)));

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void SlotInspectorSection::displayLayerModel(const LayerModel& pvm)
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
        PutLayerModelToFront cmd(iscore::IDocument::path(m_model), pvm_id);
        cmd.redo();
    });
    lay->addWidget(pb, 1, 0);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveLayerModelFromSlot{iscore::IDocument::path(m_model), pvm_id};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton, 1, 1);


    m_pvmSection->addContent(frame);
}


void SlotInspectorSection::on_layerModelCreated(
        const id_type<LayerModel>& pvmId)
{
    displayLayerModel(m_model.layerModel(pvmId));
}

void SlotInspectorSection::on_layerModelRemoved(
        const id_type<LayerModel>& pvmId)
{
    qDebug() << Q_FUNC_INFO << "TODO";

    m_pvmSection->removeAll();
    for (auto& pvm : m_model.layerModels())
    {
        if (pvm->id() != pvmId)
        {
            displayLayerModel(*pvm);
        }
    }
}


const SlotModel&SlotInspectorSection::model() const
{
    return m_model;
}
