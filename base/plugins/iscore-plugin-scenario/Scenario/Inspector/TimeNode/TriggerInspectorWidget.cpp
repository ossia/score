#include <Scenario/Commands/TimeNode/SetTrigger.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <QBoxLayout>
#include <QPushButton>
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Inspector/Expression/ExpressionEditorWidget.hpp>
#include "TriggerInspectorWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
TriggerInspectorWidget::TriggerInspectorWidget(
        const iscore::DocumentContext& doc,
        const Command::TriggerCommandFactoryList& fact,
        const TimeNodeModel& object,
        Inspector::InspectorWidgetBase* parent):
    QWidget{parent},
    m_triggerCommandFactory{fact},
    m_model {object},
    m_parent{parent}
{
    auto triglay = new QHBoxLayout{this};

    m_exprEditor = new ExpressionEditorWidget{doc, this};
    connect(m_exprEditor, &ExpressionEditorWidget::editingFinished,
            this, &TriggerInspectorWidget::on_triggerChanged);
    connect(m_model.trigger(), &TriggerModel::triggerChanged,
            m_exprEditor, &ExpressionEditorWidget::setExpression);

    m_addTrigBtn = new QPushButton{"Add Trigger"};
    m_rmTrigBtn = new QPushButton{"X"};

    triglay->addWidget(m_exprEditor);
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
    auto trig = m_exprEditor->expression();

    if(trig != m_model.trigger()->expression())
    {
        auto cmd = new Scenario::Command::SetTrigger{m_model, std::move(trig)};
        m_parent->commandDispatcher()->submitCommand(cmd);
    }
}

void TriggerInspectorWidget::createTrigger()
{
    m_exprEditor->setVisible(true);
    m_rmTrigBtn->setVisible(true);
    m_addTrigBtn->setVisible(false);

    auto cmd = m_triggerCommandFactory.make(
                   &Scenario::Command::TriggerCommandFactory::make_addTriggerCommand,
                   m_model);
    if(cmd)
        m_parent->commandDispatcher()->submitCommand(cmd);
}

void TriggerInspectorWidget::removeTrigger()
{
    m_exprEditor->setVisible(false);
    m_rmTrigBtn->setVisible(false);
    m_addTrigBtn->setVisible(true);

    auto cmd = m_triggerCommandFactory.make(
                   &Scenario::Command::TriggerCommandFactory::make_removeTriggerCommand,
                   m_model);
    if(cmd)
        m_parent->commandDispatcher()->submitCommand(cmd);
}

void TriggerInspectorWidget::on_triggerActiveChanged()
{
    bool v = m_model.trigger()->active();
    m_exprEditor->setVisible(v);
    m_rmTrigBtn->setVisible(v);
    m_addTrigBtn->setVisible(!v);
}

void TriggerInspectorWidget::HideRmButton()
{
    m_rmTrigBtn->setVisible(false);
}

void TriggerInspectorWidget::updateExpression(const State::Trigger& expr)
{
    m_exprEditor->setExpression(expr);
}
}
