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

    connect(m_model, &EventModel::localStatesChanged,
            this,    &EventInspectorWidget::updateInspector);
    connect (object, &EventModel::dateChanged,
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

    m_properties.push_back(new QLabel{tr("Condition")});
    m_properties.push_back(m_conditionLineEdit);

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_conditionLineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const QString& addr)
        {
            m_conditionLineEdit->setText(addr);
        });
        m_properties.push_back(pb);
    }

    /// Trigger
    m_triggerLineEdit = new QLineEdit{this};
    connect(m_triggerLineEdit, SIGNAL(editingFinished()),
            this,			 SLOT(on_triggerChanged()));

    m_properties.push_back(new QLabel{tr("Trigger")});
    m_properties.push_back(m_triggerLineEdit);

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_triggerLineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const QString& addr)
        {
            m_triggerLineEdit->setText(addr);
        });
        m_properties.push_back(pb);
    }

    // Separator
    m_properties.push_back(new Separator {this});

    // State
    m_addressesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{m_addressesWidget};
    m_addressesWidget->setLayout(dispLayout);

    QWidget* addAddressWidget = new QWidget{this};
    auto addLayout = new QGridLayout{addAddressWidget};

    m_stateLineEdit = new QLineEdit{addAddressWidget};



    auto ok_button = new QPushButton{"Add", addAddressWidget};
    connect(ok_button, &QPushButton::clicked,
            this,	   &EventInspectorWidget::on_addAddressClicked);
    addLayout->addWidget(m_stateLineEdit, 0, 0);
    addLayout->addWidget(ok_button, 0, 1);

    m_properties.push_back(new QLabel{"States"});
    m_properties.push_back(m_addressesWidget);
    m_properties.push_back(addAddressWidget);

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_stateLineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const QString& addr)
        {
            m_stateLineEdit->setText(addr);
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

    for(auto& plugdata : object->pluginModelList().list())
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

#include <State/Widgets/StateWidget.hpp>
void EventInspectorWidget::addState(const State& state)
{
    auto sw = new StateWidget{state, this};
    connect(sw, &StateWidget::removeMe,
            this, [&] () { removeState(state);});
    m_addresses.push_back(sw);
    m_addressesWidget->layout()->addWidget(sw);
}

#include <Inspector/InspectorWidgetList.hpp>
void EventInspectorWidget::updateDisplayedValues(
        const EventModel* event)
{
    // Cleanup
    for(auto& elt : m_addresses)
    {
        delete elt;
    }

    m_addresses.clear();

    m_prevConstraints->removeAll();
    m_nextConstraints->removeAll();

    m_date->clear();

    // DEMO
    if(event)
    {
        m_date->setText(QString::number(m_model->date().msec()));

        auto scenar = event->parentScenario();

        for(const State& state : event->states())
        {
            addState(state);
        }

        for(auto cstr : event->previousConstraints())
        {
            auto cstrBtn = new QPushButton {};
            // TODO constraint.metadata. ...
            cstrBtn->setText(QString::number(*cstr.val()));
            cstrBtn->setFlat(true);
            m_prevConstraints->addContent(cstrBtn);

            connect(cstrBtn, &QPushButton::clicked,
                    [ = ]()
            {
                selectionDispatcher()->setAndCommit(Selection{&scenar->constraint(cstr)});
            });


            // End state of previous
            const auto& constraint = event->parentScenario()->constraint(cstr);
            for(auto& proc : constraint.processes())
            {
                if(auto end = proc->endState())
                {
                    auto endWidg = InspectorWidgetList::makeInspectorWidget(end->stateName(), end, m_prevConstraints);
                    m_prevConstraints->addContent(endWidg);
                }
            }
        }

        for(auto cstr : event->nextConstraints())
        {
            auto cstrBtn = new QPushButton {};
            cstrBtn->setText(QString::number(*cstr.val()));
            cstrBtn->setFlat(true);
            m_nextConstraints->addContent(cstrBtn);

            connect(cstrBtn, &QPushButton::clicked, this,
                    [ = ]()
            {
                selectionDispatcher()->setAndCommit(Selection{&scenar->constraint(cstr)});
            });

            // Start state of next
            const auto& constraint = event->parentScenario()->constraint(cstr);
            for(auto& proc : constraint.processes())
            {
                if(auto start = proc->startState())
                {
                    auto startWidg = InspectorWidgetList::makeInspectorWidget(start->stateName(), start, m_nextConstraints);
                    m_nextConstraints->addContent(startWidg);
                }
            }
        }


        m_conditionLineEdit->setText(event->condition());
        m_triggerLineEdit->setText(event->trigger());
    }
}


using namespace iscore::IDocument;
using namespace Scenario;
void EventInspectorWidget::on_addAddressClicked()
{
    auto txt = m_stateLineEdit->text();
    // TODO Faire fonction pour parser texte en message.
    Message m; m.address = txt;
    auto cmd = new Command::AddStateToEvent{path(m_model), m};

    emit commandDispatcher()->submitCommand(cmd);
    m_stateLineEdit->clear();
}

void EventInspectorWidget::on_conditionChanged()
{
    auto txt = m_conditionLineEdit->text();

    if(txt == m_model->condition())
    {
        return;
    }

    auto cmd = new Command::SetCondition{path(m_model), txt};
    emit commandDispatcher()->submitCommand(cmd);
}

void EventInspectorWidget::on_triggerChanged()
{
    auto txt = m_triggerLineEdit->text();

    if(txt == m_model->trigger())
    {
        return;
    }

    auto cmd = new Command::SetTrigger{path(m_model), txt};
    emit commandDispatcher()->submitCommand(cmd);
}

void EventInspectorWidget::removeState(const State& state)
{
    auto cmd = new Command::RemoveStateFromEvent{path(m_model), state};
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
