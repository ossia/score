// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StatePresenter.hpp"

#include "StateModel.hpp"

#include <Process/ControlMessage.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <State/Message.hpp>
#include <State/MessageListSerialization.hpp>

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
#include <QUrl>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::StatePresenter)
namespace Scenario
{
StatePresenter::StatePresenter(
    const StateModel& model,
    const score::DocumentContext& ctx,
    QGraphicsItem* parentview,
    QObject* parent)
    : QObject{parent}, m_model{model}, m_view{new StateView{*this, parentview}}, m_ctx{ctx}
{
  // The scenario catches this :
  con(m_model.selection, &Selectable::changed, m_view, &StateView::setSelected);

  con(m_model, &StateModel::sig_statesUpdated, this, &StatePresenter::updateStateView);
  con(m_model, &StateModel::sig_controlMessagesUpdated, this, &StatePresenter::updateStateView);

  con(m_model, &StateModel::statusChanged, m_view, &StateView::setStatus);
  m_view->setStatus(m_model.status());

  connect(m_view, &StateView::startCreateMode, this, [=] {
    auto& plug
        = score::GUIAppContext().guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::Create);
  });
  connect(m_view, &StateView::startCreateGraphalMode, this, [=] {
    auto& plug
        = score::GUIAppContext().guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::CreateGraph);
  });
  connect(m_view, &StateView::startCreateSequence, this, [=] {
    auto& plug
        = score::GUIAppContext().guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    plug.editionSettings().setTool(Scenario::Tool::CreateSequence);
  });
  connect(m_view, &StateView::dropReceived, this, &StatePresenter::handleDrop);

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
  disp.setAndCommit({&m_model});
}

bool StatePresenter::isSelected() const
{
  return m_model.selection.get();
}

void StatePresenter::handleDrop(const QMimeData& mime)
{
  // If the mime data has states in it we can handle it.
  const auto& fmt = mime.formats();
  if (fmt.contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();

    auto cmd = new Command::AddMessagesToState{m_model, ml};

    CommandDispatcher<>{m_ctx.commandStack}.submit(cmd);
  }
  else if (fmt.contains(score::mime::processcontrol()))
  {
  }
  else if (fmt.contains(score::mime::layerdata()))
  {
    const auto json = readJson(mime.data(score::mime::layerdata()));
    if(!json.HasMember("Path") || !json.HasMember("Duration"))
      return;
    const auto& obj = JsonValue{json["Path"]}.to<Path<Process::ProcessModel>>();

    if (auto proc = obj.try_find(m_ctx))
    {
      std::vector<Process::ControlMessage> controls;
      proc->forEachControl([&](Process::Inlet& port, auto value) noexcept {
        controls.push_back(Process::ControlMessage{port, std::move(value)});
      });

      if (!controls.empty())
      {
        auto cmd = new Command::AddControlMessagesToState{m_model, std::move(controls)};
        CommandDispatcher<>{m_ctx.commandStack}.submit(cmd);
      }
    }
  }
  else if (mime.hasUrls())
  {
    auto scenario = dynamic_cast<ScenarioPresenter*>(parent());
    if (ossia::all_of(mime.urls(), [](const QUrl& u) {
          return QFileInfo{u.toLocalFile()}.suffix() == "cues";
        }))
    {
      State::MessageList ml;
      for (const auto& u : mime.urls())
      {
        auto path = u.toLocalFile();
        if (QFile f{path}; f.open(QIODevice::ReadOnly))
        {
          ml += JsonValue{readJson(f.readAll())}.to<State::MessageList>();
        }
      }
      if (!ml.empty())
      {
        auto cmd = new Command::AddMessagesToState{m_model, ml};
        CommandDispatcher<>{m_ctx.commandStack}.submit(cmd);
      }
    }
    else if (scenario && ossia::all_of(mime.urls(), [](const QUrl& u) {
               return QFileInfo{u.toLocalFile()}.suffix() == "layer";
             }))
    {
      if (m_model.nextInterval())
        return;

      auto path = mime.urls().first().toLocalFile();
      if (QFile f{path}; f.open(QIODevice::ReadOnly))
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
    else if (auto scenar = qobject_cast<Scenario::ProcessModel*>(m_model.parent()))
    {
      DropProcessOnState{}.drop(this->m_model, *scenar, mime, m_ctx);
    }
  }
  else
  {
    if (auto scenar = qobject_cast<Scenario::ProcessModel*>(m_model.parent()))
    {
      DropProcessOnState{}.drop(this->m_model, *scenar, mime, m_ctx);
    }
  }
}

void StatePresenter::updateStateView()
{
  m_view->setContainMessage(
      m_model.messages().rootNode().hasChildren()
      || !m_model.controlMessages().messages().empty());
}
}
