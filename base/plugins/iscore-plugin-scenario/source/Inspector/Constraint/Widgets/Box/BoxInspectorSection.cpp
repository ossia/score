#include "BoxInspectorSection.hpp"

#include "AddSlotWidget.hpp"
#include "Slot/SlotInspectorSection.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

#include "Commands/Constraint/Box/AddSlotToBox.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QtWidgets/QVBoxLayout>
#include <QFrame>
#include <QPushButton>

using namespace Scenario::Command;
#include "Commands/Constraint/RemoveBoxFromConstraint.hpp"
BoxInspectorSection::BoxInspectorSection(QString name,
                                         BoxModel* box,
                                         ConstraintInspectorWidget* parentConstraint) :
    InspectorSectionWidget {name, parentConstraint},
    m_model {box},
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

    connect(box,	&BoxModel::slotCreated,
            this,	&BoxInspectorSection::on_slotCreated);

    connect(box,	&BoxModel::slotRemoved,
            this,	&BoxInspectorSection::on_slotRemoved);

    for(auto& slot : m_model->getSlots())
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
        auto cmd = new RemoveBoxFromConstraint{iscore::IDocument::path(parentConstraint->model()), box->id()};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton);
}

void BoxInspectorSection::createSlot()
{
    auto cmd = new AddSlotToBox(
                   iscore::IDocument::path(m_model));

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void BoxInspectorSection::addSlotInspectorSection(SlotModel* slot)
{
    SlotInspectorSection* newSlot = new SlotInspectorSection {
                                    QString{"Slot.%1"} .arg(*slot->id().val()),
                                    *slot,
                                    this};

    m_slotSection->addContent(newSlot);

    m_slotsSectionWidgets[slot->id()] = newSlot;
}


void BoxInspectorSection::on_slotCreated(id_type<SlotModel> slotId)
{
    // TODO display them in the order of their position.
    // TODO issue : the box should grow of 10 more pixels for each slot.
    addSlotInspectorSection(m_model->slot(slotId));
}

void BoxInspectorSection::on_slotRemoved(id_type<SlotModel> slotId)
{
    auto ptr = m_slotsSectionWidgets[slotId];
    m_slotsSectionWidgets.erase(slotId);

    if(ptr)
    {
        ptr->deleteLater();
    }
}
