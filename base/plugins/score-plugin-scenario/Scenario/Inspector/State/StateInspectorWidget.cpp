// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateInspectorWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/Commands/TimeSync/SplitTimeSync.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>

#include <QAbstractProxyModel>
#include <QApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QString>
#include <QTableView>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QtAlgorithms>

#include <algorithm>
namespace Scenario
{

StateInspectorWidget::StateInspectorWidget(
    const StateModel& object, const score::DocumentContext& doc,
    QWidget* parent)
    : Inspector::InspectorWidgetBase{object, doc, parent,
                                     tr("State (%1)")
                                         .arg(object.metadata().getName())}
    , m_model{object}
    , m_context{doc}
    , m_commandDispatcher{m_context.commandStack}
{
  setObjectName("StateInspectorWidget");
  setParent(parent);

  updateDisplayedValues();
}

void StateInspectorWidget::updateDisplayedValues()
{
  // Cleanup
  // OPTIMIZEME
  m_properties.clear();
  auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
  SCORE_ASSERT(scenar);

  // State setup
  auto metadata = new MetadataWidget{m_model.metadata(),
                                     m_context.commandStack, &m_model, this};
  metadata->setupConnections(m_model);
  m_properties.push_back(metadata);
  m_properties.push_back(new score::HSeparator{this});

  {
    auto linkWidget = new QWidget;
    m_properties.push_back(linkWidget);
  }

  {
    auto splitEvent = new QPushButton{tr("Put in new Event"), this};
    connect(
        splitEvent, &QPushButton::clicked, this,
        &StateInspectorWidget::splitFromEvent);
    m_properties.push_back(splitEvent);
  }

  {
    auto splitNode = new QPushButton{tr("Desynchronize"), this};
    connect(
        splitNode, &QPushButton::clicked, this,
        &StateInspectorWidget::splitFromNode);
    m_properties.push_back(splitNode);
  }

  {
    auto lv = new Scenario::MessageView{m_model, this};
    lv->setModel(&m_model.messages());
    m_properties.push_back(lv);
  }

  updateAreaLayout(m_properties);
}

void StateInspectorWidget::splitFromEvent()
{
  auto scenar = dynamic_cast<const Scenario::ProcessModel*>(m_model.parent());
  if (scenar)
  {
    auto& parentEvent = scenar->events.at(m_model.eventId());
    if (parentEvent.states().size() > 1)
    {
      auto cmd = new Scenario::Command::SplitEvent{
          *scenar, m_model.eventId(), {m_model.id()}};

      m_commandDispatcher.submit(cmd);
    }
  }
}

void StateInspectorWidget::splitFromNode()
{
  auto scenar = dynamic_cast<const Scenario::ProcessModel*>(m_model.parent());
  if (scenar)
  {
    auto& ev = Scenario::parentEvent(m_model, *scenar);
    auto& tn = Scenario::parentTimeSync(m_model, *scenar);
    if (ev.states().size() > 1)
    {
      MacroCommandDispatcher<Command::SplitStateMacro> disp{
          m_commandDispatcher.stack()};

      auto cmd = new Scenario::Command::SplitEvent{
          *scenar, m_model.eventId(), {m_model.id()}};
      disp.submit(cmd);
      auto cmd2 = new Scenario::Command::SplitTimeSync{tn, {cmd->newEvent()}};
      disp.submit(cmd2);
      disp.commit();
    }
    else if (ev.states().size() == 1)
    {
      if (tn.events().size() > 1)
      {
        auto cmd
            = new Scenario::Command::SplitTimeSync{tn, {m_model.eventId()}};
        m_commandDispatcher.submit(cmd);
      }
    }
  }
}
}
