#include <State/OSSIASerializationImpl.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <JS/Qml/EditContext.hpp>

#include <Transport/DocumentPlugin.hpp>

#include <ossia/editor/scenario/time_value.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace JS
{

QString EditJsContext::portName(QObject* obj)
{
  if(auto p = qobject_cast<Process::Port*>(obj))
    return p->name();
  return {};
}

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
  ossia::apply_nonnull([&](const auto& t) {
    using type = std::decay_t<decltype(t)>;
    ret = Metadata<Json_k, type>::get();
  }, v);
  return ret;
}

void EditJsContext::play()
{
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_play_global(true);
}

void EditJsContext::playFromHere(double ms)
{
  if(!std::isfinite(ms) || ms < 0.)
  {
    qWarning() << "Score.playFromHere: not a finite, positive number of milliseconds";
    return;
  }
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_play_from_here(TimeVal::fromMsecs(ms));
}

void EditJsContext::playIntervalFromHere(QObject* interval, double ms)
{
  auto itv = qobject_cast<Scenario::IntervalModel*>(interval);
  if(!itv || !std::isfinite(ms) || ms < 0.)
  {
    qWarning() << "Score.playIntervalFromHere: needs an interval and a finite, "
                  "positive number of milliseconds";
    return;
  }
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_play_from_here(*itv, TimeVal::fromMsecs(ms));
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
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
  {
    // What the interval's play button ends in, also without a GUI
    if(auto engine = score::GUIAppContext()
                         .findGuiApplicationPlugin<Engine::ApplicationPlugin>())
      engine->execution().request_play_interval(*itv);
  }
  else if(auto state = qobject_cast<Scenario::StateModel*>(obj))
  {
    if(auto plug = score::GUIAppContext()
                       .findGuiApplicationPlugin<Scenario::ScenarioApplicationPlugin>())
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

void EditJsContext::stop(QObject* obj)
{
  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(!plug)
    return;
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
    plug->execution().request_stop_interval(*itv);
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
  // TimeVal::fromMsecs converts to int64 flicks: NaN, infinities and values
  // past that range are undefined behaviour there.
  if(!std::isfinite(dx))
  {
    qWarning() << "Score.scrub: not a finite number of milliseconds";
    return;
  }
  constexpr double max_ms
      = double(std::numeric_limits<int64_t>::max()) / ossia::flicks_per_millisecond<double>
        / 2.;
  dx = std::clamp(dx, -max_ms, max_ms);

  auto plug
      = score::GUIAppContext().findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  if(plug)
    plug->execution().request_transport_from_localtree(TimeVal::fromMsecs(dx));
}

QObject* EditJsContext::transport()
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  return doc->findPlugin<Transport::DocumentPlugin>();
}
}
