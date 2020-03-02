// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalExecutionHelpers.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalRawPtrExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/Bind.hpp>
#include <core/application/ApplicationSettings.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>

#include <wobjectimpl.h>
#include <QDebug>

#include <utility>
W_OBJECT_IMPL(Execution::IntervalRawPtrComponent)

namespace Execution
{
IntervalRawPtrComponentBase::IntervalRawPtrComponentBase(
    Scenario::IntervalModel& score_cst,
    const Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Scenario::GenericIntervalComponent<const Context>{score_cst,
                                                        ctx,
                                                        id,
                                                        "Executor::Interval",
                                                        nullptr}
{
  con(interval().duration,
      &Scenario::IntervalDurations::speedChanged,
      this,
      [&](double sp) {
        if (m_ossia_interval)
          in_exec([sp, cst = m_ossia_interval] { cst->set_speed(sp); });
      });

  con(interval().duration,
      &Scenario::IntervalDurations::defaultDurationChanged,
      this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
            cst->set_nominal_duration(t);
          });
      });

  con(interval().duration,
      &Scenario::IntervalDurations::minDurationChanged,
      this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
            cst->set_min_duration(t);
          });
      });

  con(interval().duration,
      &Scenario::IntervalDurations::maxDurationChanged,
      this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
            cst->set_max_duration(t);
          });
      });

  con(interval(), &Scenario::IntervalModel::mutedChanged, this, [&](bool b) {
    if(m_ossia_interval)
      in_exec([b, itv = m_ossia_interval] {
        itv->mute(b);
      });
  });

  con(interval(), &Scenario::IntervalModel::busChanged, this, [&](bool b) {
    if(m_ossia_interval)
      in_exec([b, itv = m_ossia_interval] {
        auto& audio_out = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.has_gain = b;
      });
  });
  con(*interval().outlet, &Process::AudioOutlet::gainChanged, this, [&](double g) {
    if(m_ossia_interval)
      in_exec([g, itv = m_ossia_interval] {
        auto& audio_out = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.gain = g;
      });
  });

  // TODO tempo, etc
}

IntervalRawPtrComponent::~IntervalRawPtrComponent() {}

void IntervalRawPtrComponent::init()
{
  if (m_interval)
  {
    if (interval().muted())
      OSSIAInterval()->mute(true);
    init_hierarchy();

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

void IntervalRawPtrComponent::cleanup(
    const std::shared_ptr<IntervalRawPtrComponent>& self)
{
  if (m_ossia_interval)
  {
    in_exec([itv = m_ossia_interval, self] {
      // self has to be kept alive until next tick
      itv->set_callback(ossia::time_interval::exec_callback{});
      itv->cleanup();
    });
    system().setup.unregister_node(
        {interval().inlet.get()},
        {interval().outlet.get()},
        m_ossia_interval->node);
  }
  for (auto& proc : m_processes)
    proc.second->cleanup();

  executionStopped();
  clear();
  m_processes.clear();
  m_ossia_interval = nullptr;
  disconnect();
}

interval_duration_data IntervalRawPtrComponentBase::makeDurations() const
{
  return {context().time(interval().duration.defaultDuration()),
          context().time(interval().duration.minDuration()),
          context().time(interval().duration.maxDuration()),
          interval().duration.speed()};
}

void IntervalRawPtrComponent::onSetup(
    std::shared_ptr<IntervalRawPtrComponent> self,
    ossia::time_interval* ossia_cst,
    interval_duration_data dur)
{
  m_ossia_interval = ossia_cst;

  auto& audio_out = static_cast<ossia::nodes::interval*>(m_ossia_interval->node.get())->audio_out;
  audio_out.has_gain = Scenario::isBus(*m_interval, context().doc);
  audio_out.gain = m_interval->outlet->gain();
  audio_out.pan = m_interval->outlet->pan();

  m_ossia_interval->set_min_duration(dur.minDuration);
  m_ossia_interval->set_max_duration(dur.maxDuration);
  m_ossia_interval->set_speed(dur.speed);
  m_ossia_interval->set_tempo_curve(tempoCurve(interval(), context()));
  m_ossia_interval->set_time_signature_map(timeSignatureMap(interval(), context()));
  m_ossia_interval->set_quarter_duration(ossia::quarter_duration<double>); // In our ideal musical world, a "quarter" is half a logical second

  if(context().doc.app.applicationSettings.gui)
  {
    std::weak_ptr<IntervalRawPtrComponent> weak_self = self;
    in_exec([weak_self, ossia_cst, &edit = system().editionQueue] {
      ossia_cst->set_stateless_callback(
          smallfun::function<void(ossia::time_value), 32>{
              [weak_self, &edit](ossia::time_value date) {
                edit.enqueue([weak_self, date] {
                  if (auto self = weak_self.lock())
                    self->slot_callback(date);
                });
              }});
    });
  }

  // set-up the interval ports
  system().setup.register_node(
      {interval().inlet.get()},
      {interval().outlet.get()},
      m_ossia_interval->node);

  init();
}

void IntervalRawPtrComponent::slot_callback(
    ossia::time_value date)
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

ossia::time_interval* IntervalRawPtrComponentBase::OSSIAInterval() const
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
  in_exec([cstr = m_ossia_interval] { cstr->stop(); });

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
    const Id<score::Component>& id,
    ProcessComponentFactory& fac,
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

      // Node registration
      if (auto& onode = plug->node)
        ctx.setup.register_node(proc, onode);

      // Selection
      QObject::connect(
          &proc.selection,
          &Selectable::changed,
          plug.get(),
          [this, n = oproc->node](bool ok) {
            in_exec([=] {
              if (n)
                n->set_logging(ok);
            });
          });

      // Looping
      oproc->set_loops(proc.loops());
      con(proc, &Process::ProcessModel::loopsChanged,
          this, [this, p=oproc] (bool b) {
        in_exec([=] { p->set_loops(b); });
      });

      oproc->set_loop_duration(system().time(proc.loopDuration()));
      con(proc, &Process::ProcessModel::loopDurationChanged,
          this, [this, p=oproc] (TimeVal t) {
        in_exec([p, t=system().time(t)] { p->set_loop_duration(t); });
      });

      oproc->set_start_offset(system().time(proc.startOffset()));
      con(proc, &Process::ProcessModel::startOffsetChanged,
          this, [this, p=oproc] (TimeVal t) {
        in_exec([p, t=system().time(t)] { p->set_start_offset(t); });
      });

      // Audio propagation
      auto reconnectOutlets = ReconnectOutlets<IntervalRawPtrComponentBase>{*this, this->OSSIAInterval()->node, proc, oproc, system().execGraph};

      connect(& proc, &Process::ProcessModel::outletsChanged,
              this, reconnectOutlets);
      reconnectOutlets();

      // Logging
      if (oproc->node)
        oproc->node->set_logging(proc.selection.get());

      in_exec(AddProcess{{}, m_ossia_interval, oproc, plug->system().execGraph, propagatedOutlets(proc.outlets())});

      connect(plug.get(), &ProcessComponent::nodeChanged,
              this, HandleNodeChange{m_ossia_interval->node, oproc, plug->system().execGraph, proc});
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

  qDebug() << "Could not create process for " << proc.metaObject()->className();
  return nullptr;
}

std::function<void()> IntervalRawPtrComponentBase::removing(
    const Process::ProcessModel& e,
    ProcessComponent& c)
{
  auto it = m_processes.find(e.id());
  if (it != m_processes.end())
  {
    if (m_ossia_interval)
    {
      in_exec([cstr = m_ossia_interval, proc = c.OSSIAProcessPtr()] {
        cstr->remove_time_process(proc.get());
      });
    }
    c.cleanup();

    return [=] { m_processes.erase(it); };
  }
  return {};
}
}
