#include <State/OSSIASerializationImpl.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <JS/Qml/EditContext.hpp>

#include <Transport/DocumentPlugin.hpp>
namespace JS
{

QString EditJsContext::valueType(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return {};
  auto port = qobject_cast<Process::ControlInlet*>(obj);
  if(!port)
    return {};

  auto& v = port->value();
  if(!v.valid())
    return {};

  QString ret;
  ossia::apply_nonnull(
      [&](const auto& t) {
    using type = std::decay_t<decltype(t)>;
    ret = Metadata<Json_k, type>::get();
      },
      v);
  return ret;
}

void EditJsContext::play()
{
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_play_global(true);
}

void EditJsContext::pause()
{
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_play_global(false);
}

void EditJsContext::resume()
{
  play();
}

void EditJsContext::play(QObject* obj)
{
  auto plug = score::GUIAppContext()
                  .findGuiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
  if(!plug)
    return;

  if(auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
  {
    plug->execution().playInterval(itv);
  }
  else if(auto state = qobject_cast<Scenario::StateModel*>(obj))
  {
    plug->execution().playState(&Scenario::parentScenario(*state), state->id());
  }
}

void EditJsContext::stop()
{
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_stop();
}

void EditJsContext::reinitialize()
{
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_reinitialize_from_localtree();
}

void EditJsContext::scrub(double dx)
{
  // TODO
}
QObject* EditJsContext::transport()
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  return doc->findPlugin<Transport::DocumentPlugin>();
}
}
