#include "EventInspectorWidget.hpp"

#include "Document/Event/EventModel.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/SetCondition.hpp"
#include "Commands/Event/RemoveStateFromEvent.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"

#include <State/State.hpp>

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QCompleter>
#include <QToolButton>

#include <Process/ScenarioModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <DeviceExplorer/DeviceCompleter.hpp>
#include <Singletons/DeviceExplorerInterface.hpp>
#include "Document/Constraint/ConstraintModel.hpp"


#include "iscore/document/DocumentInterface.hpp"
#include "core/document/DocumentModel.hpp"
#include "core/document/Document.hpp"

// TODO : pour cohérence avec les autres inspectors : Scenario ou Senario::Commands ?
EventInspectorWidget::EventInspectorWidget(EventModel* object, QWidget* parent) :
    InspectorWidgetBase {object, parent},
    m_model {object}
{
    setObjectName("EventInspectorWidget");
    setInspectedObject(m_model);
    setParent(parent);

    connect(m_model, &EventModel::messagesChanged,
            this, &EventInspectorWidget::updateInspector);

    // date
    auto dateWid = new QWidget{this};
    auto dateLay = new QHBoxLayout{dateWid};
    auto dateTitle = new QLabel{tr("default date : ")};
    m_date = new QLabel{QString::number(object->date().msec()) };
    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    m_properties.push_back(dateWid);

    // timeNode

    QWidget* tnWid = new QWidget {this};
    QHBoxLayout* tnLay = new QHBoxLayout {tnWid};
    QPushButton* tnBtn = new QPushButton {tr("None"), tnWid};
    tnBtn->setFlat(true);
    tnLay->addWidget(new QLabel{tr("TimeNode :"), tnWid});
    tnLay->addWidget(tnBtn);

    auto timeNode = m_model->timeNode();
    if(timeNode)
    {
        tnBtn->setText(QString::number(*timeNode.val()));
        auto scenar = m_model->parentScenario();
        if (scenar)
            connect(tnBtn,  &QPushButton::clicked,
                    [=] () { selectionDispatcher()->setAndCommit(Selection{scenar->timeNode(timeNode)}); });
    }

    m_properties.push_back(tnWid);

    // Condition
    m_conditionWidget = new QLineEdit{this};
    connect(m_conditionWidget, SIGNAL(editingFinished()),
            this,			 SLOT(on_conditionChanged()));

    // TODO : attention, ordre de m_properties utilisé (dans addAddress() !! faudrait changer ...
    m_properties.push_back(new QLabel{tr("Condition")});
    m_properties.push_back(m_conditionWidget);

    // State
    m_addressesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{m_addressesWidget};
    m_addressesWidget->setLayout(dispLayout);

    QWidget* addAddressWidget = new QWidget{this};
    auto addLayout = new QHBoxLayout{addAddressWidget};

    m_addressLineEdit = new QLineEdit{addAddressWidget};

    auto deviceexplorer = DeviceExplorer::getModel(m_model);

    if(deviceexplorer)
    {
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_addressLineEdit->setCompleter(completer);
    }


    auto ok_button = new QPushButton{"Add", addAddressWidget};
    connect(ok_button, &QPushButton::clicked,
            this,	   &EventInspectorWidget::on_addAddressClicked);
    addLayout->addWidget(m_addressLineEdit);
    addLayout->addWidget(ok_button);

    m_properties.push_back(new QLabel{"States"});
    m_properties.push_back(m_addressesWidget);
    m_properties.push_back(addAddressWidget);

    // Constraint list
    m_prevConstraints = new InspectorSectionWidget{tr("Previous Constraints") };
    m_nextConstraints = new InspectorSectionWidget{tr("Next Constraints") };
    m_properties.push_back(m_prevConstraints);
    m_properties.push_back(m_nextConstraints);


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

    // Display data
    updateSectionsView(areaLayout(), m_properties);
    areaLayout()->addStretch();

    // metadata
    m_metadata = new MetadataWidget{&object->metadata, commandDispatcher(), object, this};
    m_metadata->setType(EventModel::prettyName());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    // display data
    updateDisplayedValues(object);

    connect (object,   &EventModel::dateChanged,
             this,      &EventInspectorWidget::modelDateChanged);
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
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
void EventInspectorWidget::updateDisplayedValues(EventModel* event)
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
                selectionDispatcher()->setAndCommit(Selection{scenar->constraint(cstr)});
            });


            // End state of previous
            auto constraint = event->parentScenario()->constraint(cstr);
            for(auto& proc : constraint->processes())
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
                selectionDispatcher()->setAndCommit(Selection{scenar->constraint(cstr)});
            });

            // Start state of next
            auto constraint = event->parentScenario()->constraint(cstr);
            for(auto& proc : constraint->processes())
            {
                if(auto start = proc->startState())
                {
                    auto startWidg = InspectorWidgetList::makeInspectorWidget(start->stateName(), start, m_nextConstraints);
                    m_nextConstraints->addContent(startWidg);
                }
            }
        }


        m_conditionWidget->setText(event->condition());
    }
}


using namespace iscore::IDocument;
using namespace Scenario;
void EventInspectorWidget::on_addAddressClicked()
{
    auto txt = m_addressLineEdit->text();
    // TODO Faire fonction pour parser texte en message.
    Message m; m.address = txt;
    auto cmd = new Command::AddStateToEvent{path(m_model), m};

    emit commandDispatcher()->submitCommand(cmd);
    m_addressLineEdit->clear();
}

void EventInspectorWidget::on_conditionChanged()
{
    auto txt = m_conditionWidget->text();

    if(txt == m_model->condition())
    {
        return;
    }

    auto cmd = new Command::SetCondition{path(m_model), txt};
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
