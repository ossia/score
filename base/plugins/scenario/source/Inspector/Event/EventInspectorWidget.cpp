#include "EventInspectorWidget.hpp"

#include "Document/Event/EventModel.hpp"

#include "Document/Event/State/State.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/SetCondition.hpp"

#include <InspectorInterface/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QApplication>
#include <QCompleter>
#include <QToolButton>

#include "core/interface/document/DocumentInterface.hpp"
#include "base/plugins/device_explorer/DeviceInterface/DeviceCompleter.hpp"
#include "base/plugins/device_explorer/DeviceInterface/DeviceExplorerInterface.hpp"

// TODO : pour cohérence avec les autres inspectors : Scenario ou Senario::Commands ?
EventInspectorWidget::EventInspectorWidget(EventModel* object, QWidget* parent) :
    InspectorWidgetBase {nullptr},
    m_model {object}
{
    setObjectName("EventInspectorWidget");
    setInspectedObject(m_model);
    setParent(parent);

    connect(m_model, &EventModel::messagesChanged,
            this, &EventInspectorWidget::updateMessages);

    m_conditionWidget = new QLineEdit{this};
    connect(m_conditionWidget, SIGNAL(editingFinished()),
            this,			 SLOT(on_conditionChanged()));
    // TODO : attention, ordre de m_properties utilisé (dans addAddress() !! faudrait changer ...
    m_properties.push_back(new QLabel{"Condition"});
    m_properties.push_back(m_conditionWidget);

    QWidget* addressesWidget = new QWidget{this};
    auto dispLayout = new QVBoxLayout{addressesWidget};
    addressesWidget->setLayout(dispLayout);

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
    m_properties.push_back(addressesWidget);
    m_properties.push_back(addAddressWidget);

    // Constraint list
    m_prevConstraints = new InspectorSectionWidget{tr("Previous Constraints") };
    m_nextConstraints = new InspectorSectionWidget{tr("Next Constraints") };
    m_properties.push_back(m_prevConstraints);
    m_properties.push_back(m_nextConstraints);

    updateSectionsView(areaLayout(), m_properties);
    areaLayout()->addStretch();

    // metadata
    m_metadata = new MetadataWidget{&object->metadata, this};
    m_metadata->setType(EventModel::prettyName()); // TODO le faire automatiquement avec T::className
    connect(m_metadata, &MetadataWidget::submitCommand,
            this,       &InspectorWidgetBase::submitCommand);
    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    // display data
    updateDisplayedValues(object);
}

void EventInspectorWidget::addAddress(const QString& addr)
{
    auto address = new QWidget {this};
    auto lay = new QHBoxLayout {address};
    auto lbl = new QLabel {addr, this};
    lay->addWidget(lbl);

    QToolButton* rmBtn = new QToolButton {};
    rmBtn->setText("X");
    lay->addWidget(rmBtn);

    connect(rmBtn, &QToolButton::clicked,
            [ = ]()
    {
        removeState(lbl->text());
    });

    m_addresses.push_back(address);
    m_properties[3]->layout()->addWidget(address);
}

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

    // DEMO
    if(event)
    {
        //        setName (event->metadata.name());
        //		setColor (event->metadata.color() );
        //		setComments (event->metadata.comment() );

        //        setInspectedObject (event);
        //		changeLabelType ("Event");

        for(State* state : event->states())
        {
            for(auto& msg : state->messages())
            {
                addAddress(msg);
            }
        }

        for(auto cstr : event->previousConstraints())
        {
            auto cstrBtn = new QPushButton {};
            cstrBtn->setText(QString::number(*cstr.val()));
            cstrBtn->setFlat(true);
            m_prevConstraints->addContent(cstrBtn);

            connect(cstrBtn, &QPushButton::clicked,
                    [ = ]()
            {
                //TODO m_model->constraintSelected(cstrBtn->text());
            });
        }

        for(auto cstr : event->nextConstraints())
        {
            auto cstrBtn = new QPushButton {};
            cstrBtn->setText(QString::number(*cstr.val()));
            cstrBtn->setFlat(true);
            m_nextConstraints->addContent(cstrBtn);

            connect(cstrBtn, &QPushButton::clicked,
                    [ = ]()
            {
                //TODO m_model->constraintSelected(cstrBtn->text());
            });
        }


        m_conditionWidget->setText(event->condition());
    }
}


using namespace iscore::IDocument;
using namespace Scenario;
void EventInspectorWidget::on_addAddressClicked()
{
    auto txt = m_addressLineEdit->text();
    auto cmd = new Command::AddStateToEvent{path(m_model), txt};

    emit submitCommand(cmd);
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
    emit submitCommand(cmd);
}

void EventInspectorWidget::updateMessages()
{
    updateDisplayedValues(m_model);
}

void EventInspectorWidget::removeState(QString state)
{
    // TODO command de suppression
    qDebug() << "remove state" << state;
}
