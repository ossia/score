#include "SlotInspectorSection.hpp"

#include <Scenario/Inspector/Constraint/Widgets/Rack/Slot/AddLayerModelWidget.hpp>

#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <Scenario/Inspector/Constraint/Widgets/Rack/RackInspectorSection.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>

#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/RemoveLayerModelFromSlot.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>

#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QPushButton>
#include <QLabel>
using namespace Scenario::Command;

SlotInspectorSection::SlotInspectorSection(
        const QString& name,
        const SlotModel& slot,
        RackInspectorSection* parentRack) :
    InspectorSectionWidget {name, false, parentRack},
    m_model {slot},
    m_parent{parentRack->constraintInspector()}
{
    auto framewidg = new QFrame;
    auto lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    framewidg->setLayout(lay);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    // View model list
    m_lmSection = new InspectorSectionWidget{"Process View Models", false, this};
    m_lmSection->setObjectName("LayerModels");

    con(m_model.layers, &NotifyingMap<LayerModel>::added,
        this, &SlotInspectorSection::on_layerModelCreated);

    con(m_model.layers, &NotifyingMap<LayerModel>::removed,
        this, &SlotInspectorSection::on_layerModelRemoved);

    for(const auto& lm : m_model.layers)
    {
        displayLayerModel(lm);
    }

    m_addLmWidget = new AddLayerModelWidget{this};
    lay->addWidget(m_lmSection);
    lay->addWidget(m_addLmWidget);


    // Delete button
    auto deleteButton = new QPushButton{"Delete"};
    connect(deleteButton, &QPushButton::pressed, this, [&] ()
    {
        auto cmd = new RemoveSlotFromRack{m_model};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });

    lay->addWidget(deleteButton);

    connect(this, &InspectorSectionWidget::nameChanged,
            this, &SlotInspectorSection::ask_changeName);
}

void SlotInspectorSection::createLayerModel(
        const Id<Process>& sharedProcessModelId)
{
    auto cmd = new AddLayerModelToSlot{
                m_model,
                m_model.parentConstraint().processes.at(sharedProcessModelId)};

    m_parent->commandDispatcher()->submitCommand(cmd);
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
    QString name = lm.objectName();
    name.resize(name.indexOf("Layer"));
    auto id = lm.processModel().id();

    lay->addWidget(new QLabel {QString{name + ".%1"} .arg(*id.val()) }, 0, 0);

    // To front button
    auto pb = new QPushButton {tr("Front")};

    connect(pb, &QPushButton::clicked,
            [=]() {
        PutLayerModelToFront cmd{m_model, lm_id};
        cmd.redo();
    });
    lay->addWidget(pb, 1, 0);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveLayerModelFromSlot{m_model, lm_id};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton, 1, 1);


    m_lmSection->addContent(frame);
}


void SlotInspectorSection::on_layerModelCreated(
        const LayerModel& lm)
{
    displayLayerModel(lm);
}

void SlotInspectorSection::on_layerModelRemoved(const LayerModel& removed)
{
    // OPTIMIZEME
    m_lmSection->removeAll();
    for (const auto& lm : m_model.layers)
    {
        if (lm.id() != removed.id())
        {
            displayLayerModel(lm);
        }
    }
}


const SlotModel&SlotInspectorSection::model() const
{
    return m_model;
}

void SlotInspectorSection::ask_changeName(QString newName)
{

    if(newName != m_model.metadata.name())
    {
        auto cmd = new ChangeElementName<SlotModel>{m_model, newName};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    }
}
