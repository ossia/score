
#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Inspector/Expression/ExpressionEditorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Inspector/State/StateInspectorWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <State/Expression.hpp>
#include <iscore/widgets/Separator.hpp>

#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QString>
#include <QWidget>
#include <algorithm>
#include <iscore/widgets/MarginLess.hpp>

#include "EventInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Expression.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/SignalUtils.hpp>

namespace Scenario
{
EventInspectorWidget::EventInspectorWidget(
    const EventModel& object,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : QWidget{parent}
    , m_model{object}
    , m_context{doc}
    , m_commandDispatcher{doc.commandStack}
    , m_selectionDispatcher{doc.selectionStack}
    , m_menu{[&] { return m_model.condition(); }, this}
{
  setObjectName("EventInspectorWidget");
  setParent(parent);

  auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
  ISCORE_ASSERT(scenar);

  con(m_model, &EventModel::statesChanged, this,
      &EventInspectorWidget::updateDisplayedValues, Qt::QueuedConnection);

  ////// HEADER
  // metadata
  m_metadata = new MetadataWidget{m_model.metadata(), doc.commandStack,
                                  &m_model, this};
  m_metadata->setupConnections(m_model);

  m_properties.push_back(m_metadata);

  ////// BODY
  /// Information
  auto infoWidg = new QWidget;
  auto infoLay = new iscore::MarginLess<QFormLayout>{infoWidg};

  // timeNode
  auto timeNode = m_model.timeNode();
  auto tnBtn = SelectionButton::make(
      tr("Parent TimeNode"),
      &scenar->timeNode(timeNode),
      m_selectionDispatcher,
      infoWidg);

  infoLay->addWidget(tnBtn);

  m_properties.push_back(infoWidg);

  // Condition
  auto condSection
      = new Inspector::InspectorSectionWidget{"Condition", false, this};

  {
    auto expr_widg = new QWidget;
    auto expr_lay = new QHBoxLayout{expr_widg};

    m_exprEditor = new ExpressionEditorWidget{m_context, expr_widg};
    connect(
        m_exprEditor, &ExpressionEditorWidget::editingFinished, this,
        &EventInspectorWidget::on_conditionChanged);
    connect(
        m_exprEditor, &ExpressionEditorWidget::resetExpression, this,
        &EventInspectorWidget::on_conditionReset);
    con(m_model, &EventModel::conditionChanged, m_exprEditor,
        &ExpressionEditorWidget::setExpression);

    auto condMenuButton = new QPushButton{"#"};
    condMenuButton->setMenu(m_menu.menu);

    connect(m_menu.addSubAction, &QAction::triggered, m_exprEditor, [=] {
      m_exprEditor->addNewTerm();
      m_exprEditor->on_editFinished();
    });

    m_menu.menu->removeAction(m_menu.deleteAction);
    delete m_menu.deleteAction; // Blergh

    con(m_menu, &ExpressionMenu::expressionChanged, this,
        [=](const QString& str) {
          auto cond = State::parseExpression(str);
          if (!cond)
          {
            cond = State::defaultTrueExpression();
          }

          if (*cond != m_model.condition())
          {
            auto cmd = new Scenario::Command::SetCondition{m_model,
                                                           std::move(*cond)};
            m_commandDispatcher.submitCommand(cmd);
          }
        });

    expr_lay->addWidget(m_exprEditor);
    expr_lay->addWidget(condMenuButton);
    condSection->addContent(expr_widg);
  }

  // Offset
  {
    auto w = new QWidget;
    auto l = new QFormLayout{w};
    m_offsetBehavior = new QComboBox{w};
    m_offsetBehavior->addItem(
        tr("True"), QVariant::fromValue(OffsetBehavior::True));
    m_offsetBehavior->addItem(
        tr("False"), QVariant::fromValue(OffsetBehavior::False));
    m_offsetBehavior->addItem(
        tr("Expression"), QVariant::fromValue(OffsetBehavior::Expression));

    m_offsetBehavior->setCurrentIndex((int)m_model.offsetBehavior());
    con(m_model, &EventModel::offsetBehaviorChanged, this,
        [=](OffsetBehavior b) { m_offsetBehavior->setCurrentIndex((int)b); });
    connect(
        m_offsetBehavior, SignalUtils::QComboBox_currentIndexChanged_int(),
        this, [=](int idx) {
          if (idx != (int)m_model.offsetBehavior())
          {
            CommandDispatcher<> c{this->m_context.commandStack};
            c.submitCommand(
                new Command::SetOffsetBehavior{m_model, (OffsetBehavior)idx});
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

    condSection->addContent(w);
  }

  m_properties.push_back(condSection);

  condSection->expand(!m_model.condition().toString().isEmpty());

  // State
  m_statesWidget = new QWidget{this};
  auto dispLayout = new QVBoxLayout{m_statesWidget};
  m_statesWidget->setLayout(dispLayout);
  dispLayout->setSizeConstraint(QLayout::SetMinimumSize);

  m_properties.push_back(m_statesWidget);

  // Plugins (TODO factorize with ConstraintInspectorWidget)
  ISCORE_TODO;
  /*
  for(auto& plugdata : m_model.pluginModelList.list())
  {
      for(auto plugin : doc.pluginModels())
      {
          auto md = plugin->makeElementPluginWidget(plugdata, this);
          if(md)
          {
              m_properties.push_back(md);
              break;
          }
      }
  }
  */

  updateDisplayedValues();

  // Display data
  //    updateAreaLayout(m_properties);

  auto lay = new iscore::MarginLess<QVBoxLayout>{this};
  for (auto w : m_properties)
    lay->addWidget(w);
  this->setLayout(lay);
}

void EventInspectorWidget::addState(const StateModel& state)
{
  auto sw = new StateInspectorWidget{state, m_context, this};
  sw->hide(); // TODO UGLY : we create a state (inspectorbase) just to extract
              // the section ...
  auto& section = sw->stateSection();
  section.showMenu(true);
  auto split = section.menu()->addAction(tr("Put in new Event"));
  connect(
      split, &QAction::triggered, sw, &StateInspectorWidget::splitEvent,
      Qt::QueuedConnection);

  m_states.push_back(sw);
  m_statesWidget->layout()->addWidget(&section);
  m_states.push_back(&section);
  m_statesSections[state.id()] = &section;

  section.expand(false);

  con(state.selection, &Selectable::changed, this, [&](bool b) {
    if (b)
      for (auto sec : m_statesSections)
      {
        if (state.id() == sec.first)
          sec.second->expand(b);
        emit expandEventSection(b);
      }
  });
}
/*
void EventInspectorWidget::removeState(const Id<StateModel>& state)
{
    // OPTIMIZEME
    updateDisplayedValues();
}
*/

void EventInspectorWidget::focusState(const StateModel* state)
{
  ISCORE_TODO;
}

void EventInspectorWidget::updateDisplayedValues()
{
  // Cleanup
  for (auto& elt : m_states)
  {
    delete elt;
  }

  m_statesSections.clear();
  m_states.clear();

  if (!m_model.parent())
    return;

  auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
  ISCORE_ASSERT(scenar);
  for (const auto& state : m_model.states())
  {
    auto st = scenar->findState(state);
    if (st)
      addState(*st);
  }

  m_exprEditor->setExpression(m_model.condition());
}

using namespace iscore::IDocument;
using namespace Scenario;

void EventInspectorWidget::on_conditionChanged()
{
  auto cond = m_exprEditor->expression();

  if (cond != m_model.condition())
  {
    auto cmd
        = new Scenario::Command::SetCondition{path(m_model), std::move(cond)};
    emit m_commandDispatcher.submitCommand(cmd);
  }
}
void EventInspectorWidget::on_conditionReset()
{
  auto cmd
      = new Scenario::Command::SetCondition{path(m_model), State::Condition{}};
  emit m_commandDispatcher.submitCommand(cmd);
}
}
