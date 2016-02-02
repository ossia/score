#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QFrame>
#include <QPushButton>

#include "AddSlotWidget.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ModelMetadata.hpp>
#include "RackInspectorSection.hpp"
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include "Slot/SlotInspectorSection.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <Inspector/Separator.hpp>

#include <Scenario/Commands/Constraint/RemoveRackFromConstraint.hpp>
#include <algorithm>

namespace Scenario
{
RackInspectorSection::RackInspectorSection(
        const QString& name,
        const RackModel& rack,
        const ConstraintInspectorWidget& parentConstraint,
        QWidget* parent) :
    InspectorSectionWidget {name, false, parent},
    m_parent{parentConstraint},
    m_model {rack}
{
    auto framewidg = new QFrame;
    auto lay = new iscore::MarginLess<QVBoxLayout>{framewidg};
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    this->showMenu(true);
    this->enableDelete();
    connect(this, &RackInspectorSection::deletePressed, this, [=] ()
    {
        auto cmd = new Command::RemoveRackFromConstraint{
                   m_parent.model(),
                   m_model.id()};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
    });

    // Slots
    m_slotSection = new InspectorSectionWidget{"Slots", false, this};  // TODO Make a custom widget.
    m_slotSection->setObjectName("Slots");

    m_model.slotmodels.added.connect<RackInspectorSection, &RackInspectorSection::on_slotCreated>(this);
    m_model.slotmodels.removed.connect<RackInspectorSection, &RackInspectorSection::on_slotRemoved>(this);

    for(const auto& slot : m_model.slotmodels)
    {
        addSlotInspectorSection(slot);
    }

    // add indention in section
    auto indentWidg = new QWidget{this};
    auto indentLay = new iscore::MarginLess<QHBoxLayout>{indentWidg};

    indentLay->addWidget(new Inspector::VSeparator{this});
    indentLay->addWidget(m_slotSection);
    indentLay->setStretchFactor(m_slotSection, 10);

    m_slotWidget = new AddSlotWidget{this};
    lay->addWidget(indentWidg);
    lay->addWidget(m_slotWidget);

    connect(this, &InspectorSectionWidget::nameChanged,
        this, &RackInspectorSection::ask_changeName);
}

void RackInspectorSection::createSlot()
{
    auto cmd = new Command::AddSlotToRack{m_model};

    emit m_parent.commandDispatcher()->submitCommand(cmd);
}

void RackInspectorSection::ask_changeName(QString newName)
{
    if(newName != m_model.metadata.name())
    {
        auto cmd = new Command::ChangeElementName<RackModel>{m_model, newName};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
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
}
