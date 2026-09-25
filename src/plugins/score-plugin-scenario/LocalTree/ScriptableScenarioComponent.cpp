#include "ScriptableScenarioComponent.hpp"

#include <ossia/editor/scenario/time_sync.hpp>

#include <Scenario/Document/State/StateModel.hpp>


#include <score/model/ComponentUtils.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/network/base/parameter.hpp>

#include <ossia-qt/invoke.hpp>

#include <QApplication>
#include <QPointer>

namespace LocalTree
{
ScriptableTimeSync::ScriptableTimeSync(
    ScriptableRoots roots, Scenario::TimeSyncModel& sync,
    const score::DocumentContext& ctx, QObject* parent)
    : score::GenericComponent<const score::DocumentContext>{
        ctx, QStringLiteral("ScriptableTimeSyncComponent"), parent}
    , m_roots{roots}
    , m_sync{sync}
{
  announceReferrer(roots, sync);
  con(sync, &Scenario::TimeSyncModel::scriptableChanged, this, [this] { refresh(); });
  con(sync.metadata(), &score::ModelMetadata::NameChanged, this,
      [this](const QString& name) {
    // Also emitted when the namespace gave the name
    if(!m_node || m_node->get_name() == name.toStdString())
      return;
    m_given.follow(name);
    renameScriptableNode(*m_node, name, [this, name](const QString& n) {
      giveName(m_given, m_sync.metadata(), name, n);
    });
  });
  refresh();
}

ScriptableTimeSync::~ScriptableTimeSync()
{
  if(m_node)
  {
    m_trigger.reset();
    retireNode(m_roots, *m_node);
  }
}

void ScriptableTimeSync::refresh()
{
  if(m_sync.scriptable() && !m_node)
  {
    const auto own = m_sync.metadata().getName();
    m_given.follow(own);
    m_node = createScriptableNode(
        m_roots, m_roots.triggers, own,
        [this, own](const QString& n) { giveName(m_given, m_sync.metadata(), own, n); });
    if(!m_node)
      return;
    tagNode(m_roots, *m_node, m_sync);
    auto param = m_node->create_parameter(ossia::val_type::IMPULSE);
    param->set_access(ossia::access_mode::SET);
    m_trigger = std::make_unique<ParameterBinding>(*param);
    m_trigger->callback = param->add_callback(
        [s = QPointer{&m_sync}, alive = m_trigger->alive,
         exec = m_execution](const ossia::value&) {
      if(exec->trigger())
        return;
      ossia::qt::run_async(qApp, [s, alive] {
        if(*alive && s)
          s->triggeredByGui();
      });
    });
  }
  else if(!m_sync.scriptable() && m_node)
  {
    m_trigger.reset();
    retireNode(m_roots, *m_node);
    m_node = nullptr;
    m_given();
  }
}

ScriptableEvent::ScriptableEvent(
    ScriptableRoots roots, Scenario::EventModel& event,
    const score::DocumentContext& ctx, QObject* parent)
    : score::GenericComponent<const score::DocumentContext>{
        ctx, QStringLiteral("ScriptableEventComponent"), parent}
    , m_roots{roots}
    , m_event{event}
{
  announceReferrer(roots, event);
  con(event, &Scenario::EventModel::scriptableChanged, this, [this] { refresh(); });
  // Each run starts with the condition false, whatever was written before
  if(roots.tree)
    con(*roots.tree, &ScriptableTreeBase::executionStarted, this, [this] {
      if(m_node)
        if(auto param = m_node->get_parameter())
          param->push_value(false);
    });
  con(event.metadata(), &score::ModelMetadata::NameChanged, this,
      [this](const QString& name) {
    // Also emitted when the namespace gave the name
    if(!m_node || m_node->get_name() == name.toStdString())
      return;
    m_given.follow(name);
    renameScriptableNode(*m_node, name, [this, name](const QString& n) {
      giveName(m_given, m_event.metadata(), name, n);
    });
  });
  refresh();
}

ScriptableEvent::~ScriptableEvent()
{
  if(m_node)
    retireNode(m_roots, *m_node);
}

void ScriptableEvent::refresh()
{
  if(m_event.scriptable() && !m_node)
  {
    const auto own = m_event.metadata().getName();
    m_given.follow(own);
    m_node = createScriptableNode(
        m_roots, m_roots.conditions, own,
        [this, own](const QString& n) { giveName(m_given, m_event.metadata(), own, n); });
    if(!m_node)
      return;
    tagNode(m_roots, *m_node, m_event);
    auto param = m_node->create_parameter(ossia::val_type::BOOL);
    param->set_access(ossia::access_mode::BI);
    param->set_value(false);
  }
  else if(!m_event.scriptable() && m_node)
  {
    retireNode(m_roots, *m_node);
    m_node = nullptr;
    m_given();
  }
}

ScriptableInterval::ScriptableInterval(
    ScriptableRoots roots, Scenario::IntervalModel& interval,
    const score::DocumentContext& ctx, QObject* parent)
    : Scenario::GenericIntervalComponent<const score::DocumentContext>{
        interval, ctx, QStringLiteral("ScriptableIntervalComponent"), parent}
    , m_roots{roots}
{
  for(auto& proc : interval.processes)
    add(proc);
  interval.processes.mutable_added.connect<&ScriptableInterval::add>(this);
  interval.processes.removing.connect<&ScriptableInterval::remove>(this);
}

ScriptableInterval::~ScriptableInterval()
{
  for(auto& [proc, comp] : m_children)
    proc->components().remove(comp);
}

void ScriptableInterval::add(Process::ProcessModel& proc)
{
  auto comp = makeScriptableProcess(m_roots, proc, system(), this);
  proc.components().add(comp);
  m_children.emplace_back(&proc, comp);
}

bool ScriptableScenarioFactory::matches(const Process::ProcessModel& proc) const noexcept
{
  return qobject_cast<const Scenario::ProcessModel*>(&proc);
}

ScriptableProcessBase* ScriptableScenarioFactory::make(
    ScriptableRoots roots, Process::ProcessModel& proc, const score::DocumentContext& ctx,
    QObject* parent) const
{
  return new ScriptableScenario{roots, static_cast<Scenario::ProcessModel&>(proc), ctx, parent};
}

void ScriptableInterval::remove(const Process::ProcessModel& proc)
{
  auto it = ossia::find_if(m_children, [&](auto& p) { return p.first == &proc; });
  if(it == m_children.end())
    return;
  it->first->components().remove(it->second);
  m_children.erase(it);
}

ScriptableScenarioBase::ScriptableScenarioBase(
    ScriptableRoots roots, Scenario::ProcessModel& scenario,
    const score::DocumentContext& ctx, QObject* parent)
    : Process::GenericProcessComponent_T<ScriptableProcessBase, Scenario::ProcessModel>{
        roots, scenario, ctx, parent}
    , m_roots{roots}
{
}

template <>
ScriptableInterval* ScriptableScenarioBase::make<ScriptableInterval, Scenario::IntervalModel>(
    Scenario::IntervalModel& elt)
{
  return new ScriptableInterval{m_roots, elt, system(), this};
}

template <>
ScriptableEvent* ScriptableScenarioBase::make<ScriptableEvent, Scenario::EventModel>(
    Scenario::EventModel& elt)
{
  return new ScriptableEvent{m_roots, elt, system(), this};
}

template <>
ScriptableTimeSync*
ScriptableScenarioBase::make<ScriptableTimeSync, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& elt)
{
  return new ScriptableTimeSync{m_roots, elt, system(), this};
}

ScriptableState::ScriptableState(
    ScriptableRoots roots, Scenario::StateModel& state, const score::DocumentContext& ctx,
    QObject* parent)
    : score::GenericComponent<const score::DocumentContext>{
        ctx, QStringLiteral("ScriptableStateComponent"), parent}
{
  announceReferrer(roots, state);
}

template <>
ScriptableState*
ScriptableScenarioBase::make<ScriptableState, Scenario::StateModel>(Scenario::StateModel& elt)
{
  return new ScriptableState{m_roots, elt, system(), this};
}

::State::Address scriptableAddress(const Scenario::TimeSyncModel& sync)
{
  if(auto c = findComponent<ScriptableTimeSync>(sync.components()))
    if(auto n = c->node())
      return addressOfNode(*n);
  return {};
}

::State::Address scriptableAddress(const Scenario::EventModel& event)
{
  if(auto c = findComponent<ScriptableEvent>(event.components()))
    if(auto n = c->node())
      return addressOfNode(*n);
  return {};
}
}
