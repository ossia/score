#include "TriggerInspectorWidget.hpp"
#include <iscore/document/DocumentInterface.hpp>

#include "Document/TimeNode/Trigger/TriggerModel.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QPushButton>

#include "Commands/TimeNode/SetTrigger.hpp"
#include "Commands/TimeNode/AddTrigger.hpp"
#include "Commands/TimeNode/RemoveTrigger.hpp"

using namespace Scenario::Command;

TriggerInspectorWidget::TriggerInspectorWidget(const TimeNodeModel& object, InspectorWidgetBase* parent):
     m_model {object},
     m_parent{parent}
{
    auto triglay = new QHBoxLayout{this};

    m_triggerLineEdit = new QLineEdit{};
    m_triggerLineEdit->setValidator(&m_validator);

    connect(m_triggerLineEdit, &QLineEdit::editingFinished,
            this, &TriggerInspectorWidget::on_triggerChanged);

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
            this, &TriggerInspectorWidget::createTrigger );

    connect(m_rmTrigBtn, &QPushButton::released,
            this, &TriggerInspectorWidget::removeTrigger);
    connect(m_model.trigger(), &TriggerModel::activeChanged,
            this, &TriggerInspectorWidget::on_triggerActiveChanged);
}

void TriggerInspectorWidget::on_triggerChanged()
{
    auto trig = m_validator.get();

    if(*trig != m_model.trigger()->expression())
    {
        auto cmd = new Scenario::Command::SetTrigger{iscore::IDocument::path(m_model), std::move(*trig)};
        emit m_parent->commandDispatcher()->submitCommand(cmd);
    }
}

void TriggerInspectorWidget::createTrigger()
{
    m_triggerLineEdit->setVisible(true);
    m_rmTrigBtn->setVisible(true);
    m_addTrigBtn->setVisible(false);

    auto cmd = new Scenario::Command::AddTrigger{iscore::IDocument::path(m_model)};
    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void TriggerInspectorWidget::removeTrigger()
{
    m_triggerLineEdit->setVisible(false);
    m_rmTrigBtn->setVisible(false);
    m_addTrigBtn->setVisible(true);

    auto cmd = new Scenario::Command::RemoveTrigger{iscore::IDocument::path(m_model)};
    emit m_parent->commandDispatcher()->submitCommand(cmd);
}

void TriggerInspectorWidget::on_triggerActiveChanged()
{
    bool v = m_model.trigger()->active();
    m_triggerLineEdit->setVisible(v);
    m_rmTrigBtn->setVisible(v);
    m_addTrigBtn->setVisible(!v);
}

void TriggerInspectorWidget::HideRmButton()
{
    m_rmTrigBtn->setVisible(false);
}

void TriggerInspectorWidget::updateExpression(QString str)
{
    m_triggerLineEdit->setText(str);
}

