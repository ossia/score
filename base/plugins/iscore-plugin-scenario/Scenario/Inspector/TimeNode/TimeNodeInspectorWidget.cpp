#include "TimeNodeInspectorWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Commands/TimeNode/SplitTimeNode.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Inspector/Event/EventInspectorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/widgets/Separator.hpp>

#include <Inspector/InspectorWidgetBase.hpp>
#include <QApplication>
#include <QBoxLayout>
#include <QColor>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <algorithm>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/TextLabel.hpp>

struct FunctionEvent : public QEvent
{
  const std::function<void()> fun;

  template <typename Fun>
  FunctionEvent(Fun&& f) : fun(std::move(f))
  {
  }
};

// TODO put me in app context
class FunctionEventReceiver : public QObject
{
  bool event(QEvent* event) final override
  {
    if (auto e = dynamic_cast<FunctionEvent*>(event))
    {
      if (e->fun)
      {
        e->fun();
      }
    }
    return true;
  }
};

namespace Scenario
{
TimeNodeInspectorWidget::TimeNodeInspectorWidget(
    const TimeNodeModel& object,
    const iscore::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object, ctx, parent}, m_model{object}
{
  setObjectName("TimeNodeInspectorWidget");
  setParent(parent);

  // default date
  auto dateWid = new QWidget{this};
  auto dateLay = new QFormLayout{dateWid};
  m_date = new TextLabel{m_model.date().toString(), dateWid};

  dateLay->addRow(tr("Default date"), m_date);
  dateWid->setLayout(dateLay);

  // Trigger
  auto trigSec
      = new Inspector::InspectorSectionWidget{tr("Trigger"), false, this};
  m_trigwidg = new TriggerInspectorWidget{
      ctx, ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      m_model, this};
  trigSec->addContent(m_trigwidg);
  trigSec->expand(!m_model.trigger()->expression().toString().isEmpty());

  // Events
  m_events = new QWidget{this};
  auto evLay = new iscore::MarginLess<QVBoxLayout>{m_events};
  evLay->setSizeConstraint(QLayout::SetMinimumSize);

  m_properties.push_back(dateWid);
  m_properties.push_back(trigSec);
  m_properties.push_back(new TextLabel{tr("Events"), this});
  m_properties.push_back(m_events);

  updateAreaLayout(m_properties);

  // display data
  updateDisplayedValues();

  // metadata
  m_metadata = new MetadataWidget{m_model.metadata(), ctx.commandStack,
                                  &m_model, this};

  m_metadata->setupConnections(m_model);

  addHeader(m_metadata);

  con(m_model, &TimeNodeModel::dateChanged, this,
      &TimeNodeInspectorWidget::on_dateChanged);

  con(m_model, &TimeNodeModel::newEvent, this,
      [&](const Id<EventModel>&) { this->updateDisplayedValues(); });
  con(m_model, &TimeNodeModel::eventRemoved, this,
      [&](const Id<EventModel>& id) { this->removeEvent(id); });
}

void TimeNodeInspectorWidget::addEvent(const EventModel& event)
{
  auto evSection = new Inspector::InspectorSectionWidget{
      event.metadata().getName(), false, this};
  auto ew = new EventInspectorWidget{event, context(), evSection};
  evSection->addContent(ew);
  evSection->expand(false);
  evSection->showMenu(true);
  auto splitAct = evSection->menu()->addAction("Put in new Timenode");
  connect(splitAct, &QAction::triggered, this, [&]() {
    // TODO all this machinery is ugly but it crashes for some reason if
    // we just send the command directly...
    auto tn = &m_model;
    auto id = event.id();
    auto st = &commandDispatcher()->stack();
    selectionDispatcher().setAndCommit({});

    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QTimer::singleShot(0, [=] {
      // TODO we should instead not show the option in the menu
      if(tn->events().size() >= 2)
      {
        auto cmd = new Command::SplitTimeNode{*tn, {id}};

        CommandDispatcher<> s{*st};
        s.submitCommand(cmd);
      }
    });
  });

  m_eventList[event.id()] = evSection;

  m_properties.push_back(evSection);
  m_events->layout()->addWidget(evSection);

  con(event.selection, &Selectable::changed, this, [&](bool b) {
    if (!b)
      return;
    for (auto sec : m_eventList)
    {
      if (event.id() == sec.first)
        sec.second->expand(b);
    }
  });
  connect(ew, &EventInspectorWidget::expandEventSection, this, [&](bool b) {
    if (!b)
      return;
    for (auto sec : m_eventList)
    {
      if (event.id() == sec.first)
        sec.second->expand(b);
    }
  });

  con(event.metadata(), &iscore::ModelMetadata::NameChanged, this,
      [&](const QString s) {
        for (auto sec : m_eventList)
        {
          if (event.id() == sec.first)
            sec.second->renameSection(s);
        }
      });
}

void TimeNodeInspectorWidget::removeEvent(const Id<EventModel>& event)
{
  // OPTIMIZEME
  updateDisplayedValues();
}

QString TimeNodeInspectorWidget::tabName()
{
  return tr("TimeNode");
}

void TimeNodeInspectorWidget::updateDisplayedValues()
{
  // Cleanup
  // OPTIMIZEME
  for (auto& elt : m_eventList)
  {
    m_properties.remove(elt.second);
    delete elt.second;
  }
  m_eventList.clear();

  m_date->setText(m_model.date().toString());

  for (const auto& event : m_model.events())
  {
    auto scenar = dynamic_cast<ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);
    auto& evModel = scenar->event(event);
    addEvent(evModel);
  }

  m_trigwidg->updateExpression(m_model.trigger()->expression());
}

void TimeNodeInspectorWidget::on_splitTimeNodeClicked()
{
  /*
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
      auto cmd = new Command::SplitTimeNode{m_model,
                                   eventGroup};

      commandDispatcher()->submitCommand(cmd);
  }

  updateDisplayedValues();
  */
}

void TimeNodeInspectorWidget::on_dateChanged(const TimeVal& t)
{
  m_date->setText(t.toString());
}
}
