// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "BaseScenarioComponent.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/ExecutionContext.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(Execution::BaseScenarioElement)

namespace Execution
{
BaseScenarioElement::BaseScenarioElement(const Context& ctx, QObject* parent)
    : QObject{nullptr}, m_ctx{ctx}
{
}

BaseScenarioElement::~BaseScenarioElement() { }

void BaseScenarioElement::init(BaseScenarioRefContainer element)
{
  auto main_start_node = std::make_shared<ossia::time_sync>();
  auto main_end_node = std::make_shared<ossia::time_sync>();

  auto main_start_event = *main_start_node->emplace(
      main_start_node->get_time_events().begin(),
      [](auto&&...) {},
      ossia::expressions::make_expression_true());
  auto main_end_event = *main_end_node->emplace(
      main_end_node->get_time_events().begin(),
      [](auto&&...) {},
      ossia::expressions::make_expression_true());

  // TODO PlayDuration of base interval.
  // TODO PlayDuration of FullView
  auto main_interval = ossia::time_interval::create(
      {},
      *main_start_event,
      *main_end_event,
      m_ctx.time(element.interval().duration.defaultDuration()),
      m_ctx.time(element.interval().duration.minDuration()),
      m_ctx.time(element.interval().duration.maxDuration()));

  m_ossia_startTimeSync = std::make_shared<TimeSyncComponent>(
      element.startTimeSync(), m_ctx, newId(element.startTimeSync()), this);
  m_ossia_endTimeSync = std::make_shared<TimeSyncComponent>(
      element.endTimeSync(), m_ctx, newId(element.endTimeSync()), this);

  m_ossia_startEvent = std::make_shared<EventComponent>(
      element.startEvent(), m_ctx, newId(element.startEvent()), this);
  m_ossia_endEvent = std::make_shared<EventComponent>(
      element.endEvent(), m_ctx, newId(element.endEvent()), this);

  m_ossia_startState = std::make_shared<StateComponent>(
      element.startState(), main_start_event, m_ctx, newId(element.startState()), this);
  m_ossia_endState = std::make_shared<StateComponent>(
      element.endState(), main_end_event, m_ctx, newId(element.endState()), this);

  m_ossia_interval = std::make_shared<IntervalComponent>(
      element.interval(), m_ctx, newId(element.interval()), this);

  m_ossia_startTimeSync->onSetup(main_start_node, m_ossia_startTimeSync->makeTrigger());
  m_ossia_endTimeSync->onSetup(main_end_node, m_ossia_endTimeSync->makeTrigger());
  m_ossia_startEvent->onSetup(
      main_start_event,
      m_ossia_startEvent->makeExpression(),
      (ossia::time_event::offset_behavior)element.startEvent().offsetBehavior());
  m_ossia_endEvent->onSetup(
      main_end_event,
      m_ossia_endEvent->makeExpression(),
      (ossia::time_event::offset_behavior)element.endEvent().offsetBehavior());
  m_ossia_startState->onSetup();
  m_ossia_endState->onSetup();
  m_ossia_interval->onSetup(m_ossia_interval, main_interval, m_ossia_interval->makeDurations());

  for (auto dev : m_ctx.execState->edit_devices())
  {
    if (dynamic_cast<ossia::audio_protocol*>(&dev->get_protocol()))
    {
      if (auto n = ossia::net::find_node(dev->get_root_node(), "/out/main"))
      {
        if (auto param = n->get_parameter())
        {
          auto node = main_interval->node;
          m_ctx.executionQueue.enqueue([=] {
            static_cast<ossia::nodes::interval*>(node.get())->audio_out.addresses = {param};
          });
          break;
        }
      }
    }
  }
}

void BaseScenarioElement::cleanup()
{
  if (m_ossia_interval)
    m_ossia_interval->cleanup(m_ossia_interval);
  if (m_ossia_startState)
    m_ossia_startState->cleanup(m_ossia_startState);
  if (m_ossia_endState)
    m_ossia_endState->cleanup(m_ossia_startState);
  if (m_ossia_startEvent)
    m_ossia_startEvent->cleanup();
  if (m_ossia_endEvent)
    m_ossia_endEvent->cleanup();
  if (m_ossia_startTimeSync)
    m_ossia_startTimeSync->cleanup();
  if (m_ossia_endTimeSync)
    m_ossia_endTimeSync->cleanup();
  m_ossia_interval.reset();
  m_ossia_startState.reset();
  m_ossia_endState.reset();
  m_ossia_startEvent.reset();
  m_ossia_endEvent.reset();
  m_ossia_startTimeSync.reset();
  m_ossia_endTimeSync.reset();
}

IntervalComponent& BaseScenarioElement::baseInterval() const
{
  return *m_ossia_interval;
}

TimeSyncComponent& BaseScenarioElement::startTimeSync() const
{
  return *m_ossia_startTimeSync;
}

TimeSyncComponent& BaseScenarioElement::endTimeSync() const
{
  return *m_ossia_endTimeSync;
}

EventComponent& BaseScenarioElement::startEvent() const
{
  return *m_ossia_startEvent;
}

EventComponent& BaseScenarioElement::endEvent() const
{
  return *m_ossia_endEvent;
}

StateComponent& BaseScenarioElement::startState() const
{
  return *m_ossia_startState;
}

StateComponent& BaseScenarioElement::endState() const
{
  return *m_ossia_endState;
}
}

BaseScenarioRefContainer::BaseScenarioRefContainer(
    Scenario::IntervalModel& interval,
    Scenario::ScenarioInterface& s)
    : m_interval{interval}
    , m_startState{s.state(interval.startState())}
    , m_endState{s.state(interval.endState())}
    , m_startEvent{s.event(m_startState.eventId())}
    , m_endEvent{s.event(m_endState.eventId())}
    , m_startNode{s.timeSync(m_startEvent.timeSync())}
    , m_endNode{s.timeSync(m_endEvent.timeSync())}
{
}
