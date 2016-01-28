#include <Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/RemoveLayerModelFromSlot.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/Widgets/Rack/RackInspectorSection.hpp>
#include <Scenario/Inspector/Constraint/Widgets/Rack/Slot/AddLayerModelWidget.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>
#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>

#include <QPushButton>
#include <algorithm>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/LayerModel.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include "SlotInspectorSection.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>

#include <iscore/widgets/MarginLess.hpp>

#include <Inspector/Separator.hpp>

namespace Scenario
{
SlotInspectorSection::SlotInspectorSection(
        const QString& name,
        const SlotModel& slot,
        RackInspectorSection* parentRack) :
    InspectorSectionWidget {name, false, parentRack},
    m_model {slot},
    m_parent{parentRack->constraintInspector()}
{
    auto framewidg = new QFrame;
    auto lay = new iscore::MarginLess<QVBoxLayout>(framewidg);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    this->showDeleteButton(true);
    connect(this, &SlotInspectorSection::deletePressed, this, [&] ()
    {
        auto cmd = new Command::RemoveSlotFromRack{m_model};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
    });

    // View model list
    m_lmSection = new InspectorSectionWidget{"Process View Models", false, this};
    m_lmSection->setObjectName("LayerModels");

    m_model.layers.added.connect<SlotInspectorSection, &SlotInspectorSection::on_layerModelCreated>(this);
    m_model.layers.removed.connect<SlotInspectorSection, &SlotInspectorSection::on_layerModelRemoved>(this);

    for(const auto& lm : m_model.layers)
    {
        displayLayerModel(lm);
    }

    // add indention in section
    auto indentWidg = new QWidget{this};
    auto indentLay = new iscore::MarginLess<QHBoxLayout>{indentWidg};

    indentLay->addWidget(new Inspector::VSeparator{this});
    indentLay->addWidget(m_lmSection);
    indentLay->setStretchFactor(m_lmSection, 10);

    m_addLmWidget = new AddLayerModelWidget{this};
    lay->addWidget(indentWidg);
    lay->addWidget(m_addLmWidget);

    connect(this, &InspectorSectionWidget::nameChanged,
            this, &SlotInspectorSection::ask_changeName);
}

void SlotInspectorSection::createLayerModel(
        const Id<Process::ProcessModel>& sharedProcessModelId)
{
    auto cmd = new Command::AddLayerModelToSlot{
                m_model,
                m_model.parentConstraint().processes.at(sharedProcessModelId)};

    m_parent.commandDispatcher()->submitCommand(cmd);
}

void SlotInspectorSection::displayLayerModel(const Process::LayerModel& lm)
{
    auto lm_id = lm.id();

    // Layout
    auto frame = new QFrame;
    auto lay = new iscore::MarginLess<QHBoxLayout>{frame};
    frame->setFrameShape(QFrame::StyledPanel);

    // LM label
    QString name = lm.objectName();
    name.resize(name.indexOf("Layer"));
    auto id = lm.processModel().id();

    lay->addWidget(new QLabel {QString{name + ".%1"} .arg(*id.val()) });

    // To front button
    auto pb = new QPushButton {tr("Front")};

    connect(pb, &QPushButton::clicked,
            [=]() {
        PutLayerModelToFront cmd{m_model, lm_id};
        cmd.redo();
    });
    lay->addStretch(1);
    lay->addWidget(pb);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new Command::RemoveLayerModelFromSlot{m_model, lm_id};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
    });
    lay->addStretch(1);
    lay->addWidget(deleteButton);
    lay->addStretch(1);

    m_lmSection->addContent(frame);
}


void SlotInspectorSection::on_layerModelCreated(
        const Process::LayerModel& lm)
{
    displayLayerModel(lm);
}

void SlotInspectorSection::on_layerModelRemoved(const Process::LayerModel& removed)
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
        auto cmd = new Command::ChangeElementName<SlotModel>{m_model, newName};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
    }
}
}
