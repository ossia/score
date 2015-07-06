#include "EventInspectorWidget.hpp"

#include "Document/Event/EventModel.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/SetCondition.hpp"
#include "Commands/Event/SetTrigger.hpp"
#include "Commands/Event/RemoveStateFromEvent.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"

#include "base/plugins/iscore-plugin-deviceexplorer/Plugin/Panel/DeviceExplorerModel.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QFormLayout>
#include <QCompleter>

#include <Process/ScenarioModel.hpp>
#include <DeviceExplorer/../Plugin/Widgets/DeviceCompleter.hpp>
#include <DeviceExplorer/../Plugin/Widgets/DeviceExplorerMenuButton.hpp>
#include <Singletons/DeviceExplorerInterface.hpp>
#include "Document/Constraint/ConstraintModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <Inspector/Separator.hpp>

#include "core/document/DocumentModel.hpp"

// TODO : pour cohérence avec les autres inspectors : Scenario ou Senario::Commands ?
EventInspectorWidget::EventInspectorWidget(
        const EventModel* object,
        QWidget* parent) :
    InspectorWidgetBase {object, parent},
    m_model {object}
{
    setObjectName("EventInspectorWidget");
    setInspectedObject(m_model);
    setParent(parent);

    connect(m_model, &EventModel::statesChanged,
            this,    &EventInspectorWidget::updateInspector);
    connect (m_model, &EventModel::dateChanged,
             this,   &EventInspectorWidget::modelDateChanged);

    // Completion - only available if there is a device explorer
    auto deviceexplorer =
            iscore::IDocument::documentFromObject(m_model)
                ->findChild<DeviceExplorerModel*>("DeviceExplorerModel");

    ////// HEADER
    // metadata
    m_metadata = new MetadataWidget{&object->metadata, commandDispatcher(), object, this};
    m_metadata->setType(EventModel::prettyName());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);



    ////// BODY
    /// Information
    auto infoWidg = new QWidget;
    auto infoLay = new QFormLayout;
    infoWidg->setLayout(infoLay);

    // date
    m_date = new QLabel{QString::number(object->date().msec())} ;
    infoLay->addRow(tr("Default date"), m_date);

    // timeNode
    QPushButton* tnBtn = new QPushButton {tr("None")};
    tnBtn->setStyleSheet ("text-align: left");
    tnBtn->setFlat(true);

    auto timeNode = m_model->timeNode();
    if(timeNode)
    {
        tnBtn->setText(QString::number(*timeNode.val()));
        auto scenar = m_model->parentScenario();
        if (scenar)
            connect(tnBtn,  &QPushButton::clicked,
                    [=] () { selectionDispatcher()->setAndCommit(
                            Selection{&scenar->timeNode(timeNode)}); });
    }
    infoLay->addRow(tr("TimeNode"), tnBtn);

    m_properties.push_back(infoWidg);

    // Separator
    m_properties.push_back(new Separator {this});

    /// Condition
    m_conditionLineEdit = new QLineEdit{this};
    connect(m_conditionLineEdit, SIGNAL(editingFinished()),
            this,			 SLOT(on_conditionChanged()));

    m_properties.push_back(new QLabel{tr("Condition (unavailable)")});
 //   m_properties.push_back(m_conditionLineEdit); TODO uncomment when API
    m_conditionLineEdit->hide();

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_conditionLineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const iscore::Address& addr)
        {
            m_conditionLineEdit->setText(addr.toString());
        });
//       m_properties.push_back(pb); TODO uncomment when API
    }

    /// Trigger
    m_triggerLineEdit = new QLineEdit{this};
    connect(m_triggerLineEdit, SIGNAL(editingFinished()),
            this,			 SLOT(on_triggerChanged()));

    m_properties.push_back(new QLabel{tr("Trigger (unavailable)")});
//    m_properties.push_back(m_triggerLineEdit); TODO uncomment when API
    m_triggerLineEdit->hide();

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_triggerLineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const iscore::Address& addr)
        {
            m_triggerLineEdit->setText(addr.toString());
        });
//       m_properties.push_back(pb); TODO uncomment when API
    }

    // Separator
    m_properties.push_back(new Separator {this});

    // State
    m_statesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{m_statesWidget};
    m_statesWidget->setLayout(dispLayout);

    QWidget* addAddressWidget = new QWidget{this};
    auto addLayout = new QGridLayout{addAddressWidget};

    m_stateLineEdit = new QLineEdit{addAddressWidget};



    auto ok_button = new QPushButton{"Add", addAddressWidget};
    connect(ok_button, &QPushButton::clicked,
            this,	   &EventInspectorWidget::on_addAddressClicked);
    addLayout->addWidget(m_stateLineEdit, 0, 0);
    addLayout->addWidget(ok_button, 0, 1);

    m_properties.push_back(new QLabel{"States"});
    m_properties.push_back(m_statesWidget);
    m_properties.push_back(addAddressWidget);

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_stateLineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const iscore::Address& addr)
        {
            m_stateLineEdit->setText(addr.toString());
        });
        addLayout->addWidget(pb, 1, 0, 1, 2);
    }

    // Separator
    m_properties.push_back(new Separator {this});

    // Constraint list
    m_prevConstraints = new InspectorSectionWidget{tr("Previous Constraints") };
    m_properties.push_back(m_prevConstraints);
    m_nextConstraints = new InspectorSectionWidget{tr("Next Constraints") };
    m_properties.push_back(m_nextConstraints);

    // Separator
    m_properties.push_back(new Separator {this});

    // Plugins (TODO factorize with ConstraintInspectorWidget)
    iscore::Document* doc = iscore::IDocument::documentFromObject(object);

    for(auto& plugdata : object->pluginModelList.list())
    {
        for(iscore::DocumentDelegatePluginModel* plugin : doc->model()->pluginModels())
        {
            auto md = plugin->makeElementPluginWidget(plugdata, this);
            if(md)
            {
                m_properties.push_back(md);
                break;
            }
        }
    }

    updateDisplayedValues(object);


    // Display data
    updateAreaLayout(m_properties);
}

#include "Inspector/State/StateInspectorWidget.hpp"
void EventInspectorWidget::addState(const StateModel* state)
{
    auto sw = new StateInspectorWidget{state, this};
//    connect(sw, &StateInspectorWidget::removeMe,
//            this, [&] () { removeState(state);});

    m_states.push_back(sw);
    m_statesWidget->layout()->addWidget(sw);
}

void EventInspectorWidget::focusState(const StateModel* state)
{
    ISCORE_TODO
}

#include <Inspector/InspectorWidgetList.hpp>
void EventInspectorWidget::updateDisplayedValues(
        const EventModel* event)
{
    // Cleanup
    for(auto& elt : m_states)
    {
        delete elt;
    }

    m_states.clear();

    m_prevConstraints->removeAll();
    m_nextConstraints->removeAll();

    m_date->clear();

    // DEMO
    if(event)
    {
        m_date->setText(QString::number(m_model->date().msec()));

        auto scenar = event->parentScenario();
        if(!scenar)
        {
            ISCORE_TODO // Base scenario
            return;
        }

        for(const auto& state : event->states())
        {
            addState(&scenar->state(state));
        }

        m_conditionLineEdit->setText(event->condition());
        m_triggerLineEdit->setText(event->trigger());
    }
}


QVariant textToVariant(const QString& txt)
{
    bool ok = false;
    if(float val = txt.toFloat(&ok))
    {
        return val;
    }
    if(int val = txt.toInt(&ok))
    {
        return val;
    }

    return txt;
}

QVariant textToMessageValue(const QStringList& txt)
{
    if(txt.empty())
    {
        return {};
    }
    else if(txt.size() == 1)
    {
        return textToVariant(txt.first());
    }
    else
    {
        QVariantList vl;
        for(auto& elt : txt)
        {
            vl.append(textToVariant(elt));
        }
        return vl;
    }
}

using namespace iscore::IDocument;
using namespace Scenario;
void EventInspectorWidget::on_addAddressClicked()
{
    auto txt = m_stateLineEdit->text();
    auto split = txt.split(" ");

    // TODO Wrong : we should have a Message validator instead.
    // Or better, open a dialog to craft the Message by hand with different fields.
    if(iscore::Address::validateString(split.first()))
    {
        iscore::Message m;
        m.address = iscore::Address::fromString(split.first());
        split.removeFirst();
        m.value = textToMessageValue(split);

        auto cmd = new Scenario::Command::AddStateToEvent{path(m_model), m};

        emit commandDispatcher()->submitCommand(cmd);
        m_stateLineEdit->clear();
    }
}

void EventInspectorWidget::on_conditionChanged()
{
    auto txt = m_conditionLineEdit->text();

    if(txt == m_model->condition())
    {
        return;
    }

    auto cmd = new Scenario::Command::SetCondition{path(m_model), txt};
    emit commandDispatcher()->submitCommand(cmd);
}

void EventInspectorWidget::on_triggerChanged()
{
    auto txt = m_triggerLineEdit->text();

    if(txt == m_model->trigger())
    {
        return;
    }

    auto cmd = new Scenario::Command::SetTrigger{path(m_model), txt};
    emit commandDispatcher()->submitCommand(cmd);
}

void EventInspectorWidget::removeState(const iscore::State& state)
{
    auto cmd = new Scenario::Command::RemoveStateFromEvent{path(m_model), state};
    emit commandDispatcher()->submitCommand(cmd);
}

void EventInspectorWidget::updateInspector()
{
    updateDisplayedValues(m_model);
}

void EventInspectorWidget::modelDateChanged()
{
    m_date->setText(QString::number(m_model->date().msec()));
}
