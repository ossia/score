#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <boost/optional/optional.hpp>
#include <qboxlayout.h>
#include <qframe.h>
#include <qpushbutton.h>

#include "AddSlotWidget.hpp"
#include "Inspector/InspectorSectionWidget.hpp"
#include "Process/ModelMetadata.hpp"
#include "RackInspectorSection.hpp"
#include "Scenario/Document/Constraint/Rack/RackModel.hpp"
#include "Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Slot/SlotInspectorSection.hpp"
#include "iscore/command/Dispatchers/CommandDispatcher.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPath.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"
#include "iscore/tools/NotifyingMap.hpp"
#include "iscore/tools/SettableIdentifier.hpp"
#include "iscore/tools/Todo.hpp"

using namespace Scenario::Command;
#include <Scenario/Commands/Constraint/RemoveRackFromConstraint.hpp>
#include <algorithm>

RackInspectorSection::RackInspectorSection(
        const QString& name,
        const RackModel& rack,
        ConstraintInspectorWidget* parentConstraint) :
    InspectorSectionWidget {name, false, parentConstraint},
    m_parent{parentConstraint},
    m_model {rack}
{
    auto framewidg = new QFrame;
    auto lay = new QVBoxLayout; lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(0);
    framewidg->setLayout(lay);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    // Slots
    m_slotSection = new InspectorSectionWidget{"Slots", false, this};  // TODO Make a custom widget.
    m_slotSection->setObjectName("Slots");

    con(m_model.slotmodels, &NotifyingMap<SlotModel>::added,
        this, &RackInspectorSection::on_slotCreated);

    con(m_model.slotmodels, &NotifyingMap<SlotModel>::removed,
        this, &RackInspectorSection::on_slotRemoved);

    for(const auto& slot : m_model.slotmodels)
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
                   parentConstraint->model(),
                   m_model.id()};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    });
    lay->addWidget(deleteButton);

    connect(this, &InspectorSectionWidget::nameChanged,
        this, &RackInspectorSection::ask_changeName);
}

void RackInspectorSection::createSlot()
{
    auto cmd = new AddSlotToRack{m_model};

    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void RackInspectorSection::ask_changeName(QString newName)
{
    if(newName != m_model.metadata.name())
    {
        auto cmd = new ChangeElementName<RackModel>{m_model, newName};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    }
}

void RackInspectorSection::addSlotInspectorSection(const SlotModel& slot)
{
    SlotInspectorSection* newSlot = new SlotInspectorSection {
                                    slot.metadata.name(),
                                    slot,
                                    this};

    m_slotSection->addContent(newSlot);

    slotmodelsSectionWidgets[slot.id()] = newSlot;
}


void RackInspectorSection::on_slotCreated(const SlotModel& slot)
{
    // TODO display them in the order of their position.
    // TODO issue : the rack should grow of 10 more pixels for each slot.
    addSlotInspectorSection(slot);
}

void RackInspectorSection::on_slotRemoved(const SlotModel& slot)
{
    auto ptr = slotmodelsSectionWidgets[slot.id()];
    slotmodelsSectionWidgets.erase(slot.id());

    if(ptr)
    {
        ptr->deleteLater();
    }
}
