#include "SlotInspectorSection.hpp"

#include "Inspector/Constraint/Widgets/Rack/Slot/AddLayerModelWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"
#include "Inspector/Constraint/Widgets/Rack/RackInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

#include "Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp"

#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"

#include "Commands/Constraint/Rack/RemoveSlotFromRack.hpp"
#include "Commands/Constraint/Rack/Slot/RemoveLayerModelFromSlot.hpp"

#include "ViewCommands/PutLayerModelToFront.hpp"
#include <QtWidgets>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

using namespace Scenario::Command;

SlotInspectorSection::SlotInspectorSection(
        const QString& name,
        const SlotModel& slot,
        RackInspectorSection* parentRack) :
    InspectorSectionWidget {name, parentRack},
    m_model {slot},
    m_parent{parentRack->m_parent}
{
    auto framewidg = new QFrame;
    auto lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    framewidg->setLayout(lay);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    // View model list
    m_lmSection = new InspectorSectionWidget{"Process View Models", this};
    m_lmSection->setObjectName("LayerModels");

    connect(&m_model,	&SlotModel::layerModelCreated,
            this,		&SlotInspectorSection::on_layerModelCreated);

    connect(&m_model,	&SlotModel::layerModelRemoved,
            this,		&SlotInspectorSection::on_layerModelRemoved);

    for(const auto& lm : m_model.layerModels())
    {
        displayLayerModel(*lm);
    }

    m_addLmWidget = new AddLayerModelWidget{this};
    lay->addWidget(m_lmSection);
    lay->addWidget(m_addLmWidget);


    // Delete button
    auto deleteButton = new QPushButton{"Delete"};
    connect(deleteButton, &QPushButton::pressed, this, [&] ()
    {
        auto cmd = new RemoveSlotFromRack{iscore::IDocument::path(m_model)};
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

void SlotInspectorSection::displayLayerModel(const LayerModel& lm)
{
    auto lm_id = lm.id();

    // Layout
    auto frame = new QFrame;
    auto lay = new QGridLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    frame->setLayout(lay);
    frame->setFrameShape(QFrame::StyledPanel);

    // LM label
    lay->addWidget(new QLabel {QString{"ViewModel.%1"} .arg(*lm_id.val()) }, 0, 0);

    // To front button
    auto pb = new QPushButton {tr("Front")};

    connect(pb, &QPushButton::clicked,
            [=]() {
        PutLayerModelToFront cmd(iscore::IDocument::path(m_model), lm_id);
        cmd.redo();
    });
    lay->addWidget(pb, 1, 0);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveLayerModelFromSlot{iscore::IDocument::path(m_model), lm_id};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton, 1, 1);


    m_lmSection->addContent(frame);
}


void SlotInspectorSection::on_layerModelCreated(
        const id_type<LayerModel>& lmId)
{
    displayLayerModel(m_model.layerModel(lmId));
}

void SlotInspectorSection::on_layerModelRemoved(
        const id_type<LayerModel>& lmId)
{
    qDebug() << Q_FUNC_INFO << "TODO";

    m_lmSection->removeAll();
    for (auto& lm : m_model.layerModels())
    {
        if (lm->id() != lmId)
        {
            displayLayerModel(*lm);
        }
    }
}


const SlotModel&SlotInspectorSection::model() const
{
    return m_model;
}
