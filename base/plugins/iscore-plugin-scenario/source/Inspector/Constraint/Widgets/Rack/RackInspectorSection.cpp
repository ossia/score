#include "RackInspectorSection.hpp"

#include "AddSlotWidget.hpp"
#include "Slot/SlotInspectorSection.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

#include "Commands/Constraint/Rack/AddSlotToRack.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QtWidgets/QVBoxLayout>
#include <QFrame>
#include <QPushButton>

using namespace Scenario::Command;
#include "Commands/Constraint/RemoveRackFromConstraint.hpp"
RackInspectorSection::RackInspectorSection(
        const QString& name,
        const RackModel& rack,
        ConstraintInspectorWidget* parentConstraint) :
    InspectorSectionWidget {name, parentConstraint},
    m_model {rack},
    m_parent{parentConstraint}
{
    auto framewidg = new QFrame;
    auto lay = new QVBoxLayout; lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(0);
    framewidg->setLayout(lay);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    // Slots
    m_slotSection = new InspectorSectionWidget{"Slots", this};  // TODO Make a custom widget.
    m_slotSection->setObjectName("Slots");

    connect(&m_model,	&RackModel::slotCreated,
            this,	&RackInspectorSection::on_slotCreated);

    connect(&m_model,	&RackModel::slotRemoved,
            this,	&RackInspectorSection::on_slotRemoved);

    for(auto& slot : m_model.getSlots())
    {
        addSlotInspectorSection(slot);
    }

    m_slotWidget = new AddSlotWidget{this};
    lay->addWidget(m_slotSection);
    lay->addWidget(m_slotWidget);

    // Delete button
    auto deleteButton = new QPushButton{"Delete"};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new RemoveRackFromConstraint{
                   iscore::IDocument::path(parentConstraint->model()),
                   m_model.id()};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton);
}

void RackInspectorSection::createSlot()
{
    auto cmd = new AddSlotToRack(
                   iscore::IDocument::path(m_model));

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void RackInspectorSection::addSlotInspectorSection(const SlotModel& slot)
{
    SlotInspectorSection* newSlot = new SlotInspectorSection {
                                    QString{"Slot.%1"} .arg(*slot.id().val()),
                                    slot,
                                    this};

    m_slotSection->addContent(newSlot);

    m_slotsSectionWidgets[slot.id()] = newSlot;
}


void RackInspectorSection::on_slotCreated(id_type<SlotModel> slotId)
{
    // TODO display them in the order of their position.
    // TODO issue : the rack should grow of 10 more pixels for each slot.
    addSlotInspectorSection(m_model.slot(slotId));
}

void RackInspectorSection::on_slotRemoved(id_type<SlotModel> slotId)
{
    auto ptr = m_slotsSectionWidgets[slotId];
    m_slotsSectionWidgets.erase(slotId);

    if(ptr)
    {
        ptr->deleteLater();
    }
}
