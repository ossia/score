#include "TimeNodeInspectorWidget.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"

#include "Process/ScenarioModel.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include "Inspector/MetadataWidget.hpp"
#include "Inspector/Event/EventWidgets/EventShortcut.hpp"

#include "Commands/TimeNode/SplitTimeNode.hpp"
#include "Commands/TimeNode/SetTrigger.hpp"
#include "Commands/TimeNode/AddTrigger.hpp"
#include "Commands/TimeNode/RemoveTrigger.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QPushButton>
#include <QCompleter>

using namespace Scenario::Command;


TimeNodeInspectorWidget::TimeNodeInspectorWidget(
        const TimeNodeModel& object,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {object, doc, parent},
    m_model {object}
{
    setObjectName("TimeNodeInspectorWidget");
    setParent(parent);

    // default date
    QWidget* dateWid = new QWidget{this};
    QHBoxLayout* dateLay = new QHBoxLayout{dateWid};

    auto dateTitle = new QLabel{"Default date"};
    m_date = new QLabel{QString::number(m_model.date().msec()) };

    dateLay->addWidget(dateTitle);
    dateLay->addWidget(m_date);

    // Trigger
    auto trigwidg = new QWidget{this};
    auto triglay = new QHBoxLayout{trigwidg};

    m_triggerLineEdit = new QLineEdit{};
    m_triggerLineEdit->setValidator(&m_validator);

    connect(m_triggerLineEdit, &QLineEdit::editingFinished,
            this, &TimeNodeInspectorWidget::on_triggerChanged);

    connect(m_model.trigger(), &TriggerModel::triggerChanged,
        this, [this] (const iscore::Trigger& t) {
        m_triggerLineEdit->setText(t.toString());
    });

    m_addTrigBtn = new QPushButton{"Add Trigger"};
    m_rmTrigBtn = new QPushButton{"X"};

    triglay->addWidget(m_triggerLineEdit);
    triglay->addWidget(m_rmTrigBtn);
    triglay->addWidget(m_addTrigBtn);

    on_triggerActiveChanged();

    connect(m_addTrigBtn, &QPushButton::released,
            this, &TimeNodeInspectorWidget::createTrigger );

    connect(m_rmTrigBtn, &QPushButton::released,
            this, &TimeNodeInspectorWidget::removeTrigger);
    connect(m_model.trigger(), &TriggerModel::activeChanged,
            this, &TimeNodeInspectorWidget::on_triggerActiveChanged);

    // Events ids list
    m_eventList = new InspectorSectionWidget{"Events", this};

    m_properties.push_back(dateWid);
    m_properties.push_back(new QLabel{tr("Trigger")});

    m_properties.push_back(trigwidg);
    m_properties.push_back(m_eventList);

    updateAreaLayout(m_properties);

    // display data
    updateDisplayedValues();

    // metadata
    m_metadata = new MetadataWidget{&m_model.metadata, commandDispatcher(), &m_model, this};
    m_metadata->setType(TimeNodeModel::prettyName());

    m_metadata->setupConnections(m_model);

    addHeader(m_metadata);

    con(m_model, &TimeNodeModel::dateChanged,
        this,   &TimeNodeInspectorWidget::updateDisplayedValues);

    auto splitBtn = new QPushButton{this};
    splitBtn->setText("split timeNode");

    m_eventList->addContent(splitBtn);

    connect(splitBtn,   &QPushButton::clicked,
            this,       &TimeNodeInspectorWidget::on_splitTimeNodeClicked);
}


void TimeNodeInspectorWidget::updateDisplayedValues()
{
    // Cleanup
    // OPTIMIZEME
    for(auto& elt : m_events)
    {
        delete elt;
    }

    m_events.clear();

    m_date->setText(QString::number(m_model.date().msec()));

    for(const auto& event : m_model.events())
    {
        auto scenar = m_model.parentScenario();
        EventModel* evModel = &scenar->event(event);

        auto eventWid = new EventShortCut(QString::number((*event.val())));

        m_events.push_back(eventWid);
        m_eventList->addContent(eventWid);

        connect(eventWid, &EventShortCut::eventSelected,
                [=]()
        {
            selectionDispatcher().setAndCommit(Selection{evModel});
        });
    }

    m_triggerLineEdit->setText(m_model.trigger()->expression().toString());
}

void TimeNodeInspectorWidget::on_splitTimeNodeClicked()
{
    QVector<Id<EventModel> > eventGroup;

    for(const auto& ev : m_events)
    {
        if(ev->isChecked())
        {
            eventGroup.push_back( Id<EventModel>(ev->eventName().toInt()));
        }
    }

    if (eventGroup.size() < int(m_events.size()))
    {
        auto cmd = new SplitTimeNode(iscore::IDocument::path(m_model),
                                     eventGroup);

        commandDispatcher()->submitCommand(cmd);
    }

    updateDisplayedValues();
}

void TimeNodeInspectorWidget::on_triggerChanged()
{
    auto trig = m_validator.get();

    if(*trig != m_model.trigger()->expression())
    {
        auto cmd = new Scenario::Command::SetTrigger{iscore::IDocument::path(m_model), std::move(*trig)};
        emit commandDispatcher()->submitCommand(cmd);
    }
}

void TimeNodeInspectorWidget::createTrigger()
{
    m_triggerLineEdit->setVisible(true);
    m_rmTrigBtn->setVisible(true);
    m_addTrigBtn->setVisible(false);

    auto cmd = new Scenario::Command::AddTrigger{iscore::IDocument::path(m_model)};
    emit commandDispatcher()->submitCommand(cmd);
}

void TimeNodeInspectorWidget::removeTrigger()
{
    m_triggerLineEdit->setVisible(false);
    m_rmTrigBtn->setVisible(false);
    m_addTrigBtn->setVisible(true);

    auto cmd = new Scenario::Command::RemoveTrigger{iscore::IDocument::path(m_model)};
    emit commandDispatcher()->submitCommand(cmd);
}

void TimeNodeInspectorWidget::on_triggerActiveChanged()
{
    bool v = m_model.trigger()->active();
    m_triggerLineEdit->setVisible(v);
    m_rmTrigBtn->setVisible(v);
    m_addTrigBtn->setVisible(!v);
}

