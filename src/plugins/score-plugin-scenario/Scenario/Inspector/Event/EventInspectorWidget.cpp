// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "EventInspectorWidget.hpp"

#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Inspector/Expression/ExpressionEditorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Inspector/TimeSync/TriggerInspectorWidget.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Expression.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QString>
#include <QWidget>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::EventInspectorWidget)
namespace Scenario
{
EventInspectorWidget::EventInspectorWidget(
    const EventModel& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : Inspector::
        InspectorWidgetBase{object, doc, parent, tr("Event (%1)").arg(object.metadata().getName())}
    , m_model{&object}
    , m_context{doc}
    , m_commandDispatcher{doc.commandStack}
    , m_selectionDispatcher{doc.selectionStack}
    , m_menu{[&] { return object.condition(); }, this}
{
  setObjectName("EventInspectorWidget");
  setParent(parent);

  auto scenar = dynamic_cast<ScenarioInterface*>(object.parent());
  SCORE_ASSERT(scenar);

  con(object,
      &EventModel::statesChanged,
      this,
      &EventInspectorWidget::updateDisplayedValues,
      Qt::QueuedConnection);

  ////// HEADER
  // metadata
  m_metadata = new MetadataWidget{object.metadata(), doc.commandStack, &object, this};
  m_metadata->setupConnections(object);
  addHeader(m_metadata);

  ////// BODY
  /// Information
  auto infoWidg = new QWidget;
  auto infoLay = new score::MarginLess<QFormLayout>{infoWidg};

  // timeSync
  auto timeSync = object.timeSync();
  auto tnBtn = SelectionButton::make(
      tr("Parent Sync"), &scenar->timeSync(timeSync), m_selectionDispatcher, infoWidg);

  infoLay->addWidget(tnBtn);

  m_properties.push_back(infoWidg);

  m_properties.push_back(new TextLabel{tr("Condition")});
  // Condition

  {
    auto expr_widg = new QWidget{this};
    expr_widg->setContentsMargins(0, 0, 0, 0);
    auto expr_lay = new QHBoxLayout{expr_widg}; // new score::MarginLess<QHBoxLayout>{expr_widg};

    m_exprEditor = new ExpressionEditorWidget{m_context, expr_widg};
    connect(
        m_exprEditor,
        &ExpressionEditorWidget::editingFinished,
        this,
        &EventInspectorWidget::on_conditionChanged);
    connect(
        m_exprEditor,
        &ExpressionEditorWidget::resetExpression,
        this,
        &EventInspectorWidget::on_conditionReset);
    con(object,
        &EventModel::conditionChanged,
        m_exprEditor,
        &ExpressionEditorWidget::setExpression);

    m_exprEditor->setMenu(m_menu.menu);

    con(m_menu, &ExpressionMenu::expressionChanged, this, [=](const QString& str) {
      if (!m_model)
        return;
      auto cond = State::parseExpression(str);
      if (!cond)
      {
        cond = State::defaultTrueExpression();
      }

      if (*cond != m_model->condition())
      {
        auto cmd = new Scenario::Command::SetCondition{*m_model, std::move(*cond)};
        m_commandDispatcher.submit(cmd);
      }
    });

    connect(m_menu.deleteAction, &QAction::triggered, this, [=] {
      auto cmd = new Scenario::Command::SetCondition{*m_model, State::Expression{}};
      m_commandDispatcher.submit(cmd);
    });
    expr_lay->addWidget(m_exprEditor);
    m_properties.push_back(expr_widg);
  }

  // Offset
  {
    auto w = new QWidget;
    auto l = new score::MarginLess<QFormLayout>{w};
    m_offsetBehavior = new QComboBox{w};
    m_offsetBehavior->addItem(tr("True"), QVariant::fromValue(OffsetBehavior::True));
    m_offsetBehavior->addItem(tr("False"), QVariant::fromValue(OffsetBehavior::False));
    m_offsetBehavior->addItem(tr("Expression"), QVariant::fromValue(OffsetBehavior::Expression));

    m_offsetBehavior->setCurrentIndex((int)object.offsetBehavior());
    con(object, &EventModel::offsetBehaviorChanged, this, [=](OffsetBehavior b) {
      m_offsetBehavior->setCurrentIndex((int)b);
    });
    connect(
        m_offsetBehavior, SignalUtils::QComboBox_currentIndexChanged_int(), this, [=](int idx) {
          if (!m_model)
            return;
          if (idx != (int)m_model->offsetBehavior())
          {
            CommandDispatcher<> c{this->m_context.commandStack};
            c.submit(new Command::SetOffsetBehavior{*m_model, (OffsetBehavior)idx});
          }
        });

    m_offsetBehavior->setWhatsThis(
        tr("The offset behaviour is used when playing a score from "
           "the middle. \nThis allows to choose the value that the event will "
           "take, \nsince one may want to try multiple branches of conditions "
           "easily.\n"
           "Choosing 'Expression' will instead evaluate the expression."));
    m_offsetBehavior->setToolTip(m_offsetBehavior->whatsThis());
    l->addRow(tr("Offset behaviour"), m_offsetBehavior);

    m_properties.push_back(w);
  }

  updateDisplayedValues();

  updateAreaLayout(m_properties);
}

void EventInspectorWidget::updateDisplayedValues()
{
  // Cleanup
  if (!m_model || !m_model->parent())
    return;

  m_exprEditor->setExpression(m_model->condition());
}

void EventInspectorWidget::on_conditionChanged()
{
  if (!m_model)
    return;
  auto cond = m_exprEditor->expression();

  if (cond != m_model->condition())
  {
    auto cmd = new Scenario::Command::SetCondition{*m_model, std::move(cond)};
    m_commandDispatcher.submit(cmd);
  }
}
void EventInspectorWidget::on_conditionReset()
{
  if (!m_model)
    return;
  auto cmd = new Scenario::Command::SetCondition{*m_model, State::Expression{}};
  m_commandDispatcher.submit(cmd);
}
}
