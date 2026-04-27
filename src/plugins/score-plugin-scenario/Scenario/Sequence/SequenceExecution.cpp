// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioExecImpl.hpp>
#include <Scenario/Sequence/SequenceExecution.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>

#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/scenario.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Execution::SequenceComponentBase)

namespace Execution
{

SequenceComponentBase::SequenceComponentBase(
    Sequence::SequenceModel& element, const Context& ctx, QObject* parent)
    : ProcessComponent_T<Sequence::SequenceModel, ossia::scenario>{
        element, ctx, "SequenceComponent", nullptr}
    , m_ctx{ctx}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  this->setObjectName("OSSIASequenceElement");

  m_ossia_process = std::make_shared<ossia::scenario>();
  node = m_ossia_process->node;
  ((ossia::nodes::forward_node*)this->node.get())
      ->audio_in.sources.reserve(element.intervals.size() * 2);

  connect(
      this, &SequenceComponentBase::sig_eventCallback, this,
      &SequenceComponentBase::eventCallback, Qt::QueuedConnection);
}

SequenceComponentBase::~SequenceComponentBase()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

SequenceComponent::SequenceComponent(
    Sequence::SequenceModel& proc, const Context& ctx, QObject* parent)
    : SequenceComponentHierarchy{score::lazy_init_t{}, proc, ctx, parent}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

void SequenceComponent::lazy_init()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  SequenceComponentHierarchy::init();
}

void SequenceComponent::cleanup()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  clear();
  ProcessComponent::cleanup();
}

void SequenceComponentBase::stop()
{
  ScenarioExecImpl::stop_body(*this);
  ProcessComponent::stop();
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::IntervalModel& e, IntervalComponent& c)
{
  return ScenarioExecImpl::removing_interval(*this, e, c);
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::TimeSyncModel& e, TimeSyncComponent& c)
{
  return ScenarioExecImpl::removing_timesync(*this, e, c);
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::EventModel& e, EventComponent& c)
{
  return ScenarioExecImpl::removing_event(*this, e, c);
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::StateModel& e, StateComponent& c)
{
  return ScenarioExecImpl::removing_state(*this, e, c);
}

template <>
IntervalComponent*
SequenceComponentBase::make<IntervalComponent, Scenario::IntervalModel>(
    Scenario::IntervalModel& cst)
{
  return ScenarioExecImpl::make_interval(*this, cst);
}

template <>
StateComponent* SequenceComponentBase::make<StateComponent, Scenario::StateModel>(
    Scenario::StateModel& st)
{
  return ScenarioExecImpl::make_state(*this, st);
}

template <>
EventComponent* SequenceComponentBase::make<EventComponent, Scenario::EventModel>(
    Scenario::EventModel& ev)
{
  return ScenarioExecImpl::make_event(*this, ev, [](auto&) {});
}

template <>
TimeSyncComponent*
SequenceComponentBase::make<TimeSyncComponent, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& tn)
{
  return ScenarioExecImpl::make_timesync(*this, tn);
}

void SequenceComponentBase::startIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::start_interval(*this, id);
}

void SequenceComponentBase::disableIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::disable_interval(*this, id);
}

void SequenceComponentBase::stopIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::stop_interval(*this, id);
}

void SequenceComponentBase::eventCallback(
    std::weak_ptr<SequenceComponentBase> self, std::shared_ptr<EventComponent> ev,
    ossia::time_event::status newStatus)
{
  ScenarioExecImpl::event_callback(*this, self, ev, newStatus, [](auto&) {});
}

} // namespace Execution
