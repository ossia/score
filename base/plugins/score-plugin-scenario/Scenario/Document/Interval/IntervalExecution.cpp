// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/loop/loop.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <wobjectimpl.h>

#include <utility>
W_OBJECT_IMPL(Execution::IntervalComponentBase)
W_OBJECT_IMPL(Execution::IntervalComponent)

namespace Execution
{
IntervalComponentBase::IntervalComponentBase(
    Scenario::IntervalModel& score_cst, const Context& ctx,
    const Id<score::Component>& id, QObject* parent)
    : Scenario::GenericIntervalComponent<const Context>{
          score_cst, ctx, id, "Executor::Interval", nullptr}
{
  con(interval().duration, &Scenario::IntervalDurations::speedChanged, this,
      [&](double sp) {
        if (m_ossia_interval)
          in_exec([sp, cst = m_ossia_interval] { cst->set_speed(sp); });
      });

  con(interval().duration,
      &Scenario::IntervalDurations::defaultDurationChanged, this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
            cst->set_nominal_duration(t);
          });
      });

  con(interval().duration, &Scenario::IntervalDurations::minDurationChanged,
      this, [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
            cst->set_min_duration(t);
          });
      });

  con(interval().duration, &Scenario::IntervalDurations::maxDurationChanged,
      this, [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
            cst->set_max_duration(t);
          });
      });
}

IntervalComponent::~IntervalComponent()
{
}

void IntervalComponent::init()
{
  if (m_interval)
  {
    if (interval().muted())
      OSSIAInterval()->mute(true);
    init_hierarchy();

    con(interval(), &Scenario::IntervalModel::mutedChanged, this, [=](bool b) {
      in_exec([b, itv = OSSIAInterval()] { itv->mute(b); });
    });

    /* TODO put the include at the right place
    if (context().doc.app.settings<Settings::Model>().getScoreOrder())
    {
      std::vector<ossia::edge_ptr> edges_to_add;
      edges_to_add.reserve(m_processes.size());

      std::shared_ptr<ossia::graph_node> prev_node;
      for (auto& proc : m_processes)
      {
        auto& node = proc.second->OSSIAProcess().node;
        SCORE_ASSERT(node);
        if (prev_node)
        {
          edges_to_add.push_back(ossia::make_edge(
                                   ossia::dependency_connection{},
    ossia::outlet_ptr{}, ossia::inlet_ptr{}, prev_node, node));
        }
        prev_node = node;
      }

      if (prev_node)
      {
        edges_to_add.push_back(ossia::make_edge(
                                 ossia::dependency_connection{},
    ossia::outlet_ptr{}, ossia::inlet_ptr{}, prev_node,
    m_ossia_interval->node));

        std::weak_ptr<ossia::graph_interface> g_weak
            = context().execGraph;

        in_exec([edges = std::move(edges_to_add), g_weak] {
          if (auto g = g_weak.lock())
          {
            for (auto& c : edges)
            {
              g->connect(std::move(c));
            }
          }
        });
      }
    }*/
  }
}

void IntervalComponent::cleanup(const std::shared_ptr<IntervalComponent>& self)
{
  if (m_ossia_interval)
  {
    // self has to be kept alive until next tick
    in_exec([itv = m_ossia_interval, self] {
      itv->set_callback(ossia::time_interval::exec_callback{});
      itv->cleanup();
    });
    system().setup.unregister_node(
        {interval().inlet.get()}, {interval().outlet.get()},
        m_ossia_interval->node);
  }
  for (auto& proc : m_processes)
    proc.second->cleanup();

  executionStopped();
  clear();
  m_processes.clear();
  m_ossia_interval.reset();
  disconnect();
}

interval_duration_data IntervalComponentBase::makeDurations() const
{
  return {context().time(interval().duration.defaultDuration()),
          context().time(interval().duration.minDuration()),
          context().time(interval().duration.maxDuration()),
          interval().duration.speed()};
}

void IntervalComponent::onSetup(
    std::shared_ptr<IntervalComponent> self,
    std::shared_ptr<ossia::time_interval> ossia_cst,
    interval_duration_data dur)
{
  m_ossia_interval = ossia_cst;

  m_ossia_interval->set_min_duration(dur.minDuration);
  m_ossia_interval->set_max_duration(dur.maxDuration);
  m_ossia_interval->set_speed(dur.speed);

  std::weak_ptr<IntervalComponent> weak_self = self;
  in_exec([weak_self, ossia_cst, &edit = system().editionQueue] {
    ossia_cst->set_stateless_callback(
        smallfun::function<void(double, ossia::time_value), 32>{
            [weak_self, &edit](double position, ossia::time_value date) {
              edit.enqueue([weak_self, position, date] {
                if (auto self = weak_self.lock())
                  self->slot_callback(position, date);
              });
            }});
  });

  // set-up the interval ports
  system().setup.register_node(
      {interval().inlet.get()}, {interval().outlet.get()},
      m_ossia_interval->node);

  init();
}

void IntervalComponent::slot_callback(double position, ossia::time_value date)
{
  if (m_ossia_interval)
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

const std::shared_ptr<ossia::time_interval>&
IntervalComponentBase::OSSIAInterval() const
{
  return m_ossia_interval;
}

Scenario::IntervalModel& IntervalComponentBase::scoreInterval() const
{
  return interval();
}

void IntervalComponentBase::pause()
{
  m_ossia_interval->pause();
}

void IntervalComponentBase::resume()
{
  m_ossia_interval->resume();
}

void IntervalComponentBase::stop()
{
  in_exec([cstr = m_ossia_interval] { cstr->stop(); });

  for (auto& process : m_processes)
  {
    process.second->stop();
  }
  interval().reset();

  executionStopped();
}

void IntervalComponentBase::executionStarted()
{
  interval().duration.setPlayPercentage(0);
  interval().executionStarted();
  for (Process::ProcessModel& proc : interval().processes)
  {
    proc.startExecution();
  }
}

void IntervalComponentBase::executionStopped()
{
  interval().executionStopped();
  for (Process::ProcessModel& proc : interval().processes)
  {
    proc.stopExecution();
  }
}

ProcessComponent* IntervalComponentBase::make(
    const Id<score::Component>& id, ProcessComponentFactory& fac,
    Process::ProcessModel& proc)
{
  try
  {
    const Execution::Context& ctx = system();
    auto plug = fac.make(proc, ctx, id, nullptr);
    if (plug && plug->OSSIAProcessPtr())
    {
      const auto& oproc = plug->OSSIAProcessPtr();
      SCORE_ASSERT(m_processes.find(proc.id()) == m_processes.end());
      m_processes.emplace(proc.id(), plug);
      SCORE_ASSERT(m_processes[proc.id()] == plug);

      const auto& outlets = proc.outlets();
      ossia::pod_vector<std::size_t> propagated_outlets;
      for (std::size_t i = 0; i < outlets.size(); i++)
      {
        if (outlets[i]->propagate())
          propagated_outlets.push_back(i);
      }

      if (auto& onode = plug->node)
        ctx.setup.register_node(proc, onode);

      auto cst = m_ossia_interval;

      QObject::connect(
          &proc.selection, &Selectable::changed, plug.get(),
          [this, n = oproc->node](bool ok) {
            in_exec([=] {
              if (n)
                n->set_logging(ok);
            });
          });
      if (oproc->node)
        oproc->node->set_logging(proc.selection.get());

      std::weak_ptr<ossia::time_process> oproc_weak = oproc;
      std::weak_ptr<ossia::graph_interface> g_weak = plug->system().execGraph;
      std::weak_ptr<ossia::graph_node> cst_node_weak = cst->node;

      in_exec(
          [cst = m_ossia_interval, oproc_weak, g_weak, propagated_outlets] {
            if (auto oproc = oproc_weak.lock())
              if (auto g = g_weak.lock())
              {
                cst->add_time_process(oproc);
                if (oproc->node)
                {
                  ossia::graph_node& n = *oproc->node;
                  for (std::size_t propagated : propagated_outlets)
                  {
                    const auto& outlet = n.outputs()[propagated]->data;
                    if (outlet.target<ossia::audio_port>())
                    {
                      auto cable = ossia::make_edge(
                          ossia::immediate_glutton_connection{},
                          n.outputs()[propagated], cst->node->inputs()[0],
                          oproc->node, cst->node);
                      g->connect(cable);
                    }
                  }
                }
              }
          });

      connect(
          plug.get(), &ProcessComponent::nodeChanged, this,
          [cst_node_weak, g_weak, oproc_weak, &proc] (const auto& old_node, const auto& new_node, auto& commands) {
            const auto& outlets = proc.outlets();
            ossia::int_vector propagated_outlets;
            for (std::size_t i = 0; i < outlets.size(); i++)
            {
              if (outlets[i]->propagate())
                propagated_outlets.push_back(i);
            }

            commands.push_back([cst_node_weak, g_weak, propagated_outlets, old_node, new_node] {
              auto cst_node = cst_node_weak.lock();
              if(!cst_node)
                return;
              auto g = g_weak.lock();
              if(!g)
                return;

              // Remove edges from the old node
              if(old_node)
              {
                ossia::graph_node& n = *old_node;
                for(auto& outlet : n.outputs())
                {
                  auto targets = outlet->targets;
                  for(auto e : targets)
                  {
                    if(e->in_node.get() == cst_node.get())
                    {
                      g->disconnect(e);
                    }
                  }
                }
              }

              // Add edges to the new node
              if(new_node)
              {
                ossia::graph_node& n = *new_node;
                for (int propagated : propagated_outlets)
                {
                  const auto& outlet = n.outputs()[propagated]->data;
                  if (outlet.target<ossia::audio_port>())
                  {
                    auto cable = ossia::make_edge(
                          ossia::immediate_glutton_connection{},
                          n.outputs()[propagated], cst_node->inputs()[0],
                        new_node, cst_node);
                    g->connect(cable);
                  }
                }
              }
            });
          });
      return plug.get();
    }
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

std::function<void()> IntervalComponentBase::removing(
    const Process::ProcessModel& e, ProcessComponent& c)
{
  auto it = m_processes.find(e.id());
  if (it != m_processes.end())
  {
    auto c_ptr = c.shared_from_this();
    if (m_ossia_interval)
    {
      in_exec([cstr = m_ossia_interval, c_ptr] {
        cstr->remove_time_process(c_ptr->OSSIAProcessPtr().get());
      });
    }
    c.cleanup();

    return [=] { m_processes.erase(it); };
  }
  return {};
}
}
