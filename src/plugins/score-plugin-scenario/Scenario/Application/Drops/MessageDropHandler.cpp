// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageDropHandler.hpp"

#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace Scenario
{
MessageDropHandler::MessageDropHandler()
{
  m_acceptableMimeTypes.push_back(score::mime::messagelist());
  m_acceptableSuffixes.push_back("cues");
}

bool MessageDropHandler::drop(
    const Scenario::ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  using namespace Scenario::Command;

  // If the mime data has states in it we can handle it.
  State::MessageList ml;
  if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    ml = des.deserialize();
  }
  else if (mime.hasUrls())
  {
    for (const auto& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if (QFile f{path};
          QFileInfo{f}.suffix() == "cues" && f.open(QIODevice::ReadOnly))
      {
        State::MessageList sub;
        auto json = readJson(f.readAll());
        JSONWriter wr{json};
        sub <<= JsonValue{wr.base};
        ml += sub;
      }
    }
  }

  if (ml.empty())
    return false;

  Scenario::Command::Macro m{new Scenario::Command::CreateStateMacro,
                             pres.context().context};

  const Scenario::ProcessModel& scenar = pres.model();
  Id<StateModel> createdState;

  Scenario::Point pt = pres.toScenarioPoint(pos);

  m_magnetic = magneticStates(m_magnetic, pt, pres);
  auto [x_state, y_state, magnetic] = m_magnetic;
  if(y_state)
  {
    if(magnetic)
    {
      createdState = m.createState(scenar, y_state->eventId(), pt.y).id();
    }
    else
    {
      auto& s = m.createState(scenar, y_state->eventId(), pt.y);
      auto& i = m.createIntervalAfter(scenar, s.id(), pt);
      createdState = i.endState();
    }
  }
  else if (x_state)
  {
    if (x_state->nextInterval())
    {
      // We create from the event instead
      auto& s = m.createState(scenar, x_state->eventId(), pt.y);
      auto& i = m.createIntervalAfter(scenar, s.id(), pt);
      createdState = i.endState();
    }
    else
    {
      if(magnetic)
      {
        auto& i = m.createIntervalAfter(
              scenar, x_state->id(), {pt.date, x_state->heightPercentage()});
          createdState = i.endState();
      }
      else
      {
        auto& s = m.createState(scenar, x_state->eventId(), pt.y);
        auto& i = m.createIntervalAfter(scenar, s.id(), pt);
        createdState = i.endState();
      }
    }
  }
  else
  {
    // We create in the emptiness
    const auto& [t, e, s] = m.createDot(scenar, pt);
    createdState = s.id();
  }

  m.addMessages(scenar.state(createdState), std::move(ml));

  m.commit();
  return true;
}
}
