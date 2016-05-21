#include <Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/RemoveLayerModelFromSlot.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/Widgets/Rack/RackInspectorSection.hpp>
#include <Scenario/Inspector/Constraint/Widgets/Rack/Slot/AddLayerModelWidget.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
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
#include <iscore/widgets/SignalUtils.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <Inspector/Separator.hpp>

namespace Scenario
{
SlotInspectorSection::SlotInspectorSection(
        const QString& name,
        const SlotModel& slot,
        RackInspectorSection* parentRack) :
    InspectorSectionWidget {name, false, parentRack},
    m_model {slot},
    m_parent{parentRack->constraintInspector()},
    m_sb{slot, parentRack->constraintInspector().commandDispatcher()->stack(), nullptr}
{
    auto framewidg = new QFrame;
    auto lay = new iscore::MarginLess<QVBoxLayout>(framewidg);
    framewidg->setFrameShape(QFrame::StyledPanel);
    addContent(framewidg);

    this->showMenu(true);
    auto del = this->menu()->addAction(tr("Remove Slot"));
    connect(del, &QAction::triggered, this, [=] ()
    {
        auto cmd = new Command::RemoveSlotFromRack{m_model};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
    });

    // Change height
    lay->addWidget(new QLabel{tr("Slot height: ")});
    lay->addWidget(m_sb.widget());

    // View model list
    m_lmSection = new InspectorSectionWidget{"Process View Models", false, this};
    m_lmSection->setObjectName("LayerModels");

    m_model.layers.added.connect<SlotInspectorSection, &SlotInspectorSection::on_layerModelCreated>(this);
    m_model.layers.removed.connect<SlotInspectorSection, &SlotInspectorSection::on_layerModelRemoved>(this);


    // add indention in section
    auto indentWidg = new QWidget{this};
    auto indentLay = new iscore::MarginLess<QHBoxLayout>{indentWidg};

    indentLay->addWidget(new Inspector::VSeparator{this});
    indentLay->addWidget(m_lmSection);
    indentLay->setStretchFactor(m_lmSection, 10);

    m_addLmWidget = new AddLayerModelWidget{this};
    lay->addWidget(indentWidg);
    lay->addWidget(m_addLmWidget);

    auto frame = new QFrame{this};
    m_lmGridLayout = new iscore::MarginLess<QGridLayout>{frame};
    frame->setFrameShape(QFrame::StyledPanel);
    m_lmSection->addContent(frame);


    connect(this, &InspectorSectionWidget::nameChanged,
            this, &SlotInspectorSection::ask_changeName);

    for(const auto& lm : m_model.layers)
    {
        displayLayerModel(lm);
    }
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

    // LM label
    QString name = lm.objectName();
    name.resize(name.indexOf("Layer"));
    auto id = lm.processModel().id();

    auto row = m_lmGridLayout->rowCount();

    auto label = new QLabel {QString{name + ".%1"} .arg(*id.val()), this};
    m_lmGridLayout->addWidget(label, row ,0);

    // To front button
    auto pb = new QPushButton {tr("Front")};

    connect(pb, &QPushButton::clicked,
            [=]() {
        PutLayerModelToFront cmd{m_model, lm_id};
        cmd.redo();
    });
    m_lmGridLayout->addWidget(new QWidget{this}, row, 1);
    m_lmGridLayout->addWidget(pb, row, 2);
    m_lmGridLayout->addWidget(new QWidget{this}, row, 3);

    // Delete button
    auto deleteButton = new QPushButton{{tr("Delete")}};
    connect(deleteButton, &QPushButton::pressed, this, [=] ()
    {
        auto cmd = new Command::RemoveLayerModelFromSlot{m_model, lm_id};
        emit m_parent.commandDispatcher()->submitCommand(cmd);
    });
    m_lmGridLayout->addWidget(deleteButton, row, 4);
    m_lmGridLayout->addWidget(new QWidget{this}, row, 5);
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

    auto frame = new QFrame{this};
    m_lmGridLayout = new iscore::MarginLess<QGridLayout>{frame};
    frame->setFrameShape(QFrame::StyledPanel);

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
