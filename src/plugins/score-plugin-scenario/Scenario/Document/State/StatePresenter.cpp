// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StatePresenter.hpp"

#include <Scenario/Application/Drops/DropLayerInInterval.hpp>
#include <Scenario/Application/Drops/PresetDrop.hpp>

#include "StateModel.hpp"

#include <State/Message.hpp>
#include <State/MessageListSerialization.hpp>

#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/Drops/DropProcessOnState.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Scenario/Commands/State/SnapshotProcess.hpp>

#include <LocalTree/ScriptableReference.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QPointer>
#include <QUrl>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::StatePresenter)
namespace Scenario
{
StatePresenter::StatePresenter(
    const StateModel& model, const score::DocumentContext& ctx,
    QGraphicsItem* parentview, QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_view{new StateView{*this, parentview}}
    , m_ctx{ctx}
{
  // The scenario catches this :
  con(m_model.selection, &Selectable::changed, m_view, &StateView::setSelected);

  con(m_model, &StateModel::sig_statesUpdated, this, &StatePresenter::updateStateView);

  con(m_model, &StateModel::statusChanged, m_view, &StateView::setStatus);

  if(auto tree = ctx.findPlugin<LocalTree::ScriptableTreeBase>())
  {
    m_view->setEmphasized(tree->emphasized(model));
    con(*tree, &LocalTree::ScriptableTreeBase::emphasisChanged, this,
        [this, tree] { m_view->setEmphasized(tree->emphasized(m_model)); });
    // The presenter can be deleted later than its state
    con(*tree, &LocalTree::ScriptableTreeBase::referencesChanged, this,
        [this, model = QPointer<const StateModel>{&model}](QObject* referrer) {
      if(model && (!referrer || referrer == model.data()))
        updateBrokenReferences();
    });
  }
  m_view->setStatus(m_model.status());

  connect(m_view, &StateView::startCreateMode, this, [=] {
    auto& plug = score::GUIAppContext()
                     .guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::Create);
  });
  connect(m_view, &StateView::startCreateGraphalMode, this, [=] {
    auto& plug = score::GUIAppContext()
                     .guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::CreateGraph);
  });
  connect(m_view, &StateView::startCreateSequence, this, [=] {
    auto& plug = score::GUIAppContext()
                     .guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::CreateSequence);
  });
  connect(m_view, &StateView::dropReceived, this, &StatePresenter::handleDrop);

  model.stateProcesses.added.connect<&StatePresenter::on_processAdded>(this);
  model.stateProcesses.removed.connect<&StatePresenter::on_processRemoved>(this);
  updateStateView();
}

StatePresenter::~StatePresenter() { }

const Id<StateModel>& StatePresenter::id() const
{
  return m_model.id();
}

StateView* StatePresenter::view() const
{
  return m_view;
}

const StateModel& StatePresenter::model() const
{
  return m_model;
}

void StatePresenter::select() const
{
  score::SelectionDispatcher disp{m_ctx.selectionStack};
  disp.select(m_model);
}

bool StatePresenter::isSelected() const
{
  return m_model.selection.get();
}

void StatePresenter::handleDrop(const QMimeData& mime)
{
  // If the mime data has states in it we can handle it.
  const auto& fmt = mime.formats();
  if(fmt.contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();

    auto cmd = new Command::AddMessagesToState{m_model, ml};

    CommandDispatcher<>{m_ctx.commandStack}.submit(cmd);
  }
  else if(fmt.contains(score::mime::port()))
  {
    if(auto item = Dataflow::PortItem::clickedPort)
      if(auto control = qobject_cast<const Process::ControlInlet*>(&item->port()))
        if(score::IDocument::try_documentFromObject(*control) == &m_ctx.document)
          Command::snapshotControlInState(m_model, *control, m_ctx);
  }
  else if(fmt.contains(score::mime::layerdata()))
  {
    auto json = readJson(mime.data(score::mime::layerdata()));
    auto processes = draggedProcesses(json, m_ctx);
    if(processes.empty() && !draggedCopy(json) && json.HasMember("Path"))
    {
      const auto path = JsonValue{json["Path"]}.to<Path<Process::ProcessModel>>();
      if(auto proc = path.try_find(m_ctx))
        processes.push_back(proc);
    }
    if(!processes.empty())
      if(auto choice = choosePresetDrop(processes, false))
        Command::snapshotProcessesInState(
            m_model, processes, m_ctx, *choice == PresetDrop::ControlsAndState);
  }
  else if(mime.hasUrls())
  {
    auto scenario = dynamic_cast<ScenarioPresenter*>(parent());
    if(ossia::all_of(mime.urls(), [](const QUrl& u) {
         return QFileInfo{u.toLocalFile()}.suffix() == "cues";
       }))
    {
      State::MessageList ml;
      for(const auto& u : mime.urls())
      {
        auto path = u.toLocalFile();
        if(QFile f{path}; f.open(QIODevice::ReadOnly))
        {
          ossia::insert_at_end(
              ml, JsonValue{readJson(f.readAll())}.to<State::MessageList>());
        }
      }
      if(!ml.empty())
      {
        auto cmd = new Command::AddMessagesToState{m_model, ml};
        CommandDispatcher<>{m_ctx.commandStack}.submit(cmd);
      }
    }
    else if(scenario && ossia::all_of(mime.urls(), [](const QUrl& u) {
              return QFileInfo{u.toLocalFile()}.suffix() == "layer";
            }))
    {
      if(m_model.nextInterval())
        return;

      auto path = mime.urls().first().toLocalFile();
      if(QFile f{path}; f.open(QIODevice::ReadOnly))
      {
        const auto& ctx = scenario->context().context;
        auto json = readJson(f.readAll());
        Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, ctx};

        // Create a box.
        const Scenario::ProcessModel& scenar = scenario->model();

        const TimeVal t = TimeVal::fromMsecs(json["Duration"].GetDouble());

        auto& interval = m.createIntervalAfter(
            scenar, m_model.id(), Scenario::Point{t, m_model.heightPercentage()});

        DropLayerInInterval::perform(interval, ctx, m, json);
        m.commit();
      }
    }
    else if(auto scenar = qobject_cast<Scenario::ProcessModel*>(m_model.parent()))
    {
      DropProcessOnState{}.drop(this->m_model, *scenar, mime, m_ctx);
    }
  }
  else
  {
    if(auto scenar = qobject_cast<Scenario::ProcessModel*>(m_model.parent()))
    {
      DropProcessOnState{}.drop(this->m_model, *scenar, mime, m_ctx);
    }
  }
}

void StatePresenter::on_processAdded(const Process::ProcessModel&)
{
  updateStateView();
}
void StatePresenter::on_processRemoved(const Process::ProcessModel&)
{
  updateStateView();
}

void StatePresenter::updateStateView()
{
  m_view->setContainMessage(m_model.messages().rootNode().hasChildren());
  updateBrokenReferences();
  m_view->setContainProcess(!m_model.stateProcesses.empty());
}

void StatePresenter::updateBrokenReferences()
{
  bool broken = false;
  auto visit = [&](auto& self, const Process::MessageNode& n) -> void {
    if(!broken && n.values.userValue
       && LocalTree::broken(Process::address(n).address, m_ctx))
      broken = true;
    for(auto& child : n)
      self(self, child);
  };
  visit(visit, m_model.messages().rootNode());
  m_view->setBrokenReferences(broken);
}
}
