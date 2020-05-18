// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalExecutionHelpers.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <QDebug>

#include <wobjectimpl.h>

#include <utility>
W_OBJECT_IMPL(Execution::IntervalComponent)

namespace Execution
{
IntervalComponentBase::IntervalComponentBase(
    Scenario::IntervalModel& score_cst,
    const Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Scenario::GenericIntervalComponent<const Context>{
        score_cst,
        ctx,
        id,
        "Executor::Interval",
        nullptr}
{
  if (score_cst.graphal())
    return;

  con(interval().duration, &Scenario::IntervalDurations::speedChanged, this, [&](double sp) {
    if (m_ossia_interval)
      in_exec([sp, cst = m_ossia_interval] { cst->set_speed(sp); });
  });

  con(interval().duration,
      &Scenario::IntervalDurations::defaultDurationChanged,
      this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] { cst->set_nominal_duration(t); });
      });

  con(interval().duration,
      &Scenario::IntervalDurations::minDurationChanged,
      this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] { cst->set_min_duration(t); });
      });

  con(interval().duration,
      &Scenario::IntervalDurations::maxDurationChanged,
      this,
      [&](TimeVal sp) {
        if (m_ossia_interval)
          in_exec([t = ctx.time(sp), cst = m_ossia_interval] { cst->set_max_duration(t); });
      });

  con(interval(), &Scenario::IntervalModel::mutedChanged, this, [&](bool b) {
    if (m_ossia_interval)
      in_exec([b, itv = m_ossia_interval] { itv->mute(b); });
  });

  con(interval(), &Scenario::IntervalModel::busChanged, this, [&](bool b) {
    if (m_ossia_interval)
      in_exec([b, itv = m_ossia_interval] {
        auto& audio_out = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.has_gain = b;
      });
  });
  con(*interval().outlet, &Process::AudioOutlet::gainChanged, this, [&](double g) {
    if (m_ossia_interval)
      in_exec([g, itv = m_ossia_interval] {
        auto& audio_out = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.gain = g;
      });
  });
  con(*interval().outlet, &Process::AudioOutlet::panChanged, this, [&](ossia::pan_weight pan) {
    if (m_ossia_interval)
      in_exec([pan = std::move(pan), itv = m_ossia_interval] {
        auto& audio_out = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.pan = pan;
      });
  });
  // TODO tempo, etc
}

IntervalComponent::~IntervalComponent() { }

void IntervalComponent::init()
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
        {interval().inlet.get()}, {interval().outlet.get()}, m_ossia_interval->node);
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
  using namespace ossia;
  if (interval().graphal())
    return {0_tv, 0_tv, ossia::Infinite, 1.};
  else
    return {
        context().time(interval().duration.defaultDuration()),
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
  Scenario::TempoProcess* tempo_proc{};

  if (!interval().graphal())
  {
    auto& audio_out
        = static_cast<ossia::nodes::interval*>(m_ossia_interval->node.get())->audio_out;
    audio_out.has_gain = Scenario::isBus(*m_interval, context().doc);
    audio_out.gain = m_interval->outlet->gain();
    audio_out.pan = m_interval->outlet->pan();

    m_ossia_interval->set_min_duration(dur.minDuration);
    m_ossia_interval->set_max_duration(dur.maxDuration);
    m_ossia_interval->set_speed(dur.speed);
    auto tdata = tempoCurve(interval(), context());
    tempo_proc = tdata.second;
    m_ossia_interval->set_tempo_curve(std::move(tdata).first);
    m_ossia_interval->set_time_signature_map(timeSignatureMap(interval(), context()));
    m_ossia_interval->set_quarter_duration(
        ossia::quarter_duration<double>); // In our ideal musical world, a
                                          // "quarter" is half a logical second
  }
  else
  {
    using namespace ossia;
    m_ossia_interval->set_min_duration(0_tv);
    m_ossia_interval->set_max_duration(ossia::Infinite);
    m_ossia_interval->graphal = true;
  }

  if (context().doc.app.applicationSettings.gui)
  {
    std::weak_ptr<IntervalComponent> weak_self = self;

    if (Q_UNLIKELY(interval().graphal()))
    {
      in_exec([weak_self, ossia_cst, &edit = system().editionQueue] {
        ossia_cst->set_stateless_callback(smallfun::function<void(bool, ossia::time_value), 32>{
            [weak_self, &edit](bool running, ossia::time_value date) {
              edit.enqueue([weak_self, running, date] {
                if (auto self = weak_self.lock())
                  self->graph_slot_callback(running, date);
              });
            }});
      });
    }
    else
    {
      in_exec([weak_self, ossia_cst, &edit = system().editionQueue] {
        ossia_cst->set_stateless_callback(smallfun::function<void(bool, ossia::time_value), 32>{
            [weak_self, &edit](bool running, ossia::time_value date) {
              edit.enqueue([weak_self, running, date] {
                if (auto self = weak_self.lock())
                  self->slot_callback(running, date);
              });
            }});
      });
    }
  }

  // set-up the interval ports
  Process::Inlets toRegister;
  toRegister.push_back(interval().inlet.get());
  if (tempo_proc)
  {
    toRegister.push_back(tempo_proc->inlet.get());
  }

  if(interval().outlet)
  {
    system().setup.register_node(toRegister, {interval().outlet.get()}, m_ossia_interval->node);
  }

  init();
}

void IntervalComponent::graph_slot_callback(bool running, ossia::time_value date)
{
  interval().setExecuting(running);
}

void IntervalComponent::slot_callback(bool running, ossia::time_value date)
{
  if (m_ossia_interval)
  {
    if (running)
    {
      auto& cstdur = interval().duration;
      const auto& maxdur = cstdur.maxDuration();

      auto currentTime = this->context().reverseTime(date);
      if (!maxdur.infinite())
      {
        if (maxdur > TimeVal::zero())
          cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
      }
      else
      {
        if (cstdur.defaultDuration() > TimeVal::zero())
          cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
      }
    }
    interval().setExecuting(running);
  }
}

const std::shared_ptr<ossia::time_interval>& IntervalComponentBase::OSSIAInterval() const
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
    proc.benchmark(-1.);
  }
}

void IntervalComponentBase::executionStopped()
{
  interval().executionStopped();
  for (Process::ProcessModel& proc : interval().processes)
  {
    proc.stopExecution();
    proc.benchmark(-1.);
  }
}

ProcessComponent* IntervalComponentBase::make(
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
          &proc.selection, &Selectable::changed, plug.get(), [this, n = oproc->node](bool ok) {
            in_exec([=] {
              if (n)
                n->set_logging(ok);
            });
          });

      // Looping
      oproc->set_loops(proc.loops());
      con(proc, &Process::ProcessModel::loopsChanged, this, [this, p = oproc](bool b) {
        in_exec([=] { p->set_loops(b); });
      });

      oproc->set_loop_duration(system().time(proc.loopDuration()));
      con(proc, &Process::ProcessModel::loopDurationChanged, this, [this, p = oproc](TimeVal t) {
        in_exec([p, t = system().time(t)] { p->set_loop_duration(t); });
      });

      oproc->set_start_offset(system().time(proc.startOffset()));
      con(proc, &Process::ProcessModel::startOffsetChanged, this, [this, p = oproc](TimeVal t) {
        in_exec([p, t = system().time(t)] { p->set_start_offset(t); });
      });

      // Audio propagation
      auto reconnectOutlets = ReconnectOutlets<IntervalComponentBase>{
          *this, this->OSSIAInterval()->node, proc, oproc, system().execGraph};

      con(proc, &Process::ProcessModel::outletsChanged, this, reconnectOutlets);
      reconnectOutlets();

      // Logging
      if (oproc->node)
        oproc->node->set_logging(proc.selection.get());

      in_exec(AddProcess{
          m_ossia_interval,
          m_ossia_interval.get(),
          oproc,
          system().execGraph,
          propagatedOutlets(proc.outlets())});

      connect(
          plug.get(),
          &ProcessComponent::nodeChanged,
          this,
          HandleNodeChange{m_ossia_interval->node, oproc, system().execGraph, proc});
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

std::function<void()>
IntervalComponentBase::removing(const Process::ProcessModel& e, ProcessComponent& c)
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
