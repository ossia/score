// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/state/state_element.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <Automation/AutomationModel.hpp>
#include <Engine/OSSIA2score.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <utility>

#include "IntervalRawPtrComponent.hpp"
#include "Loop/LoopProcessModel.hpp"
#include "ScenarioComponent.hpp"
#include <ossia/editor/scenario/time_value.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

namespace Engine
{
namespace Execution
{
IntervalRawPtrComponentBase::IntervalRawPtrComponentBase(
    Scenario::IntervalModel& score_cst,
    const Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Scenario::GenericIntervalComponent<const Context>{score_cst, ctx, id, "Executor::Interval", nullptr}
{
  con(interval().duration,
      &Scenario::IntervalDurations::executionSpeedChanged, this,
      [&](double sp) {
    if(m_ossia_interval)
      in_exec([sp,cst = m_ossia_interval] {
        cst->set_speed(sp);
      });
  });

  con(interval().duration,
      &Scenario::IntervalDurations::defaultDurationChanged, this,
      [&](TimeVal sp) {
    if(m_ossia_interval)
      in_exec([t=ctx.time(sp),cst = m_ossia_interval]
      { cst->set_nominal_duration(t); });
  });

  con(interval().duration,
      &Scenario::IntervalDurations::minDurationChanged, this,
      [&](TimeVal sp) {
    if(m_ossia_interval)
      in_exec([t=ctx.time(sp),cst = m_ossia_interval]
      { cst->set_min_duration(t); });
  });

  con(interval().duration,
      &Scenario::IntervalDurations::maxDurationChanged, this,
      [&](TimeVal sp) {
    if(m_ossia_interval)
      in_exec([t=ctx.time(sp),cst = m_ossia_interval]
      { cst->set_max_duration(t); });
  });
}

IntervalRawPtrComponent::~IntervalRawPtrComponent()
{

}

void IntervalRawPtrComponent::init()
{
  init_hierarchy();
}

void IntervalRawPtrComponent::cleanup(const std::shared_ptr<IntervalRawPtrComponent>& self)
{
  if(m_ossia_interval)
  {
    in_exec([itv=m_ossia_interval,self] {
      // self has to be kept alive until next tick
      itv->set_callback(ossia::time_interval::exec_callback{});
      itv->cleanup();
    });
    system().plugin.unregister_node(
        {interval().inlet.get()},
        {interval().outlet.get()},
        m_ossia_interval->node);
  }
  for(auto& proc : m_processes)
    proc.second->cleanup();

  executionStopped();
  clear();
  m_processes.clear();
  m_ossia_interval = nullptr;
  disconnect();
}

interval_duration_data IntervalRawPtrComponentBase::makeDurations() const
{
  return {
        context().time(interval().duration.defaultDuration()),
        context().time(interval().duration.minDuration()),
        context().time(interval().duration.maxDuration()),
        interval().duration.executionSpeed()
  };
}

void IntervalRawPtrComponent::onSetup(
    std::shared_ptr<IntervalRawPtrComponent> self,
    ossia::time_interval* ossia_cst,
    interval_duration_data dur,
    bool parent_is_base_scenario)
{
  m_ossia_interval = ossia_cst;

  m_ossia_interval->set_min_duration(dur.minDuration);
  m_ossia_interval->set_max_duration(dur.maxDuration);
  m_ossia_interval->set_speed(dur.speed);

  // BaseScenario needs a special callback. It is given in DefaultClockManager.
  if (!parent_is_base_scenario)
  {
    std::weak_ptr<IntervalRawPtrComponent> weak_self = self;
    in_exec([weak_self,ossia_cst,&edit=system().editionQueue] {
      ossia_cst->set_stateless_callback(smallfun::function<void (double, ossia::time_value), 32>{
            [weak_self,&edit](double position, ossia::time_value date) {
        edit.enqueue([weak_self,position,date] {
          if(auto self = weak_self.lock())
            self->slot_callback(position, date);
        });
      }});
    });
  }

  // set-up the interval ports
  system().plugin.register_node(
      {interval().inlet.get()},
      {interval().outlet.get()},
      m_ossia_interval->node);

  init();
}

void IntervalRawPtrComponent::slot_callback(double position, ossia::time_value date)
{
  if(m_ossia_interval)
  {
    auto currentTime = this->context().reverseTime(date);

    auto& cstdur = interval().duration;
    const auto& maxdur = cstdur.maxDuration();

    if (!maxdur.isInfinite())
      cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
      cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
  }
}

ossia::time_interval*
IntervalRawPtrComponentBase::OSSIAInterval() const
{
  return m_ossia_interval;
}

Scenario::IntervalModel& IntervalRawPtrComponentBase::scoreInterval() const
{
  return interval();
}

void IntervalRawPtrComponentBase::pause()
{
  m_ossia_interval->pause();
}

void IntervalRawPtrComponentBase::resume()
{
  m_ossia_interval->resume();
}

void IntervalRawPtrComponentBase::stop()
{
  in_exec([cstr=m_ossia_interval] { cstr->stop(); });

  for (auto& process : m_processes)
  {
    process.second->stop();
  }
  interval().reset();

  interval().duration.setPlayPercentage(0);
  executionStopped();
}

void IntervalRawPtrComponentBase::executionStarted()
{
  interval().duration.setPlayPercentage(0);
  interval().executionStarted();
  for (Process::ProcessModel& proc : interval().processes)
  {
    proc.startExecution();
  }
}

void IntervalRawPtrComponentBase::executionStopped()
{
  interval().executionStopped();
  for (Process::ProcessModel& proc : interval().processes)
  {
    proc.stopExecution();
  }
}

ProcessComponent* IntervalRawPtrComponentBase::make(
    const Id<score::Component> & id,
    ProcessComponentFactory& fac,
    Process::ProcessModel &proc)
{
  try
  {
    const Engine::Execution::Context& ctx = system();
    auto plug = fac.make(proc, ctx, id, nullptr);
    if (plug && plug->OSSIAProcessPtr())
    {
      auto oproc = plug->OSSIAProcessPtr();
      m_processes.emplace(proc.id(), plug);

      const auto& outlets = proc.outlets();
      std::vector<int> propagated_outlets;
      for(std::size_t i = 0; i < outlets.size(); i++)
      {
        if(outlets[i]->propagate())
          propagated_outlets.push_back(i);
      }

      if(auto& onode = plug->node)
        ctx.plugin.register_node(proc, onode);

      auto cst = m_ossia_interval;

      QObject::connect(&proc.selection, &Selectable::changed,
                       plug.get(), [this,oproc] (bool ok) {
        in_exec([=] {
          if(const auto& n = oproc->node)
            n->set_logging(ok);
        });
      });
      if(oproc->node)
        oproc->node->set_logging(proc.selection.get());

      std::weak_ptr<ossia::time_process> oproc_weak = oproc;
      std::weak_ptr<ossia::graph_interface> g_weak = plug->system().plugin.execGraph;
      std::weak_ptr<ossia::graph_node> cst_node_weak = cst->node;

      in_exec(
            [cst=m_ossia_interval,oproc_weak,g_weak,propagated_outlets] {
        if(auto oproc = oproc_weak.lock())
        if(auto g = g_weak.lock())
        {
          cst->add_time_process(oproc);
          if(oproc->node)
          {
            ossia::graph_node& n = *oproc->node;
            for(int propagated : propagated_outlets)
            {
              const auto& outlet = n.outputs()[propagated]->data;
              if(outlet.target<ossia::audio_port>())
              {
                auto cable = ossia::make_edge(
                      ossia::immediate_glutton_connection{}
                      , n.outputs()[propagated]
                      , cst->node->inputs()[0]
                      , oproc->node
                      , cst->node);
                g->connect(cable);
              }
            }
          }
        }
      });


      connect(plug.get(), &ProcessComponent::nodeChanged,
              this, [this,cst_node_weak,g_weak,oproc_weak,&proc] (auto old_node, auto new_node) {

        const auto& outlets = proc.outlets();
        std::vector<int> propagated_outlets;
        for(std::size_t i = 0; i < outlets.size(); i++)
        {
          if(outlets[i]->propagate())
            propagated_outlets.push_back(i);
        }

        in_exec(
              [cst_node_weak,g_weak,oproc_weak,propagated_outlets] {
          if(auto cst_node = cst_node_weak.lock())
          if(auto g = g_weak.lock())
          if(auto oproc = oproc_weak.lock())
          if(oproc->node)
          {
            ossia::graph_node& n = *oproc->node;
            for(int propagated : propagated_outlets)
            {
              const auto& outlet = n.outputs()[propagated]->data;
              if(outlet.target<ossia::audio_port>())
              {
                auto cable = ossia::make_edge(
                               ossia::immediate_glutton_connection{}
                               , n.outputs()[propagated]
                               , cst_node->inputs()[0]
                               , oproc->node
                               , cst_node);
                g->connect(cable);
              }
            }
          }
        });

      });
    }
    return plug.get();
  }
  catch (const std::exception& e)
  {
    qDebug() << "Error while creating a process: " << e.what();
  }
  catch (...)
  {
    qDebug() << "Error while creating a process";
  }
  return nullptr;
}

std::function<void ()> IntervalRawPtrComponentBase::removing(
    const Process::ProcessModel& e,
    ProcessComponent& c)
{
  auto it = m_processes.find(e.id());
  if(it != m_processes.end())
  {
    auto c_ptr = c.shared_from_this();
    in_exec([cstr=m_ossia_interval,c_ptr] {
      cstr->remove_time_process(c_ptr->OSSIAProcessPtr().get());
    });
    c.cleanup();

    return [=] { m_processes.erase(it); };
  }
  return {};
}

}
}
