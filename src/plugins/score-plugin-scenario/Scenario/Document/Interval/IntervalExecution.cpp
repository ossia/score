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

#include <score/application/GUIApplicationContext.hpp>
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
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <QDebug>
#include <QTimer>

#include <wobjectimpl.h>

#include <utility>
W_OBJECT_IMPL(Execution::IntervalComponent)

namespace Execution
{
namespace
{
static std::pair<Process::Inlets, Process::Outlets>
portsToRegister(const Scenario::IntervalModel& itv)
{
  Scenario::TempoProcess* tempo_proc = itv.tempoCurve();

  Process::Inlets inputs;
  if(itv.inlet)
  {
    inputs.push_back(itv.inlet.get());
  }

  if(tempo_proc)
  {
    inputs.push_back(tempo_proc->tempo_inlet.get());
    inputs.push_back(tempo_proc->speed_inlet.get());
    inputs.push_back(tempo_proc->position_inlet.get());
  }

  Process::Outlets outputs;
  if(itv.outlet)
  {
    outputs.push_back(itv.outlet.get());
  }

  return {std::move(inputs), std::move(outputs)};
}
}

IntervalComponentBase::IntervalComponentBase(
    Scenario::IntervalModel& score_cst, const std::shared_ptr<ossia::scenario>& scenar,
    const Context& ctx, QObject* parent)
    : Scenario::GenericIntervalComponent<const Context>{
        score_cst, ctx, "Executor::Interval", nullptr}
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(score_cst.graphal())
    return;

  con(interval().duration, &Scenario::IntervalDurations::speedChanged, this,
      [&](double sp) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([sp, cst = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        cst->set_speed(sp);
      });
  });

  con(interval().duration, &Scenario::IntervalDurations::defaultDurationChanged, this,
      [&](TimeVal sp) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        cst->set_nominal_duration(t);
      });
  });

  con(interval().duration, &Scenario::IntervalDurations::minDurationChanged, this,
      [&](TimeVal sp) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        cst->set_min_duration(t);
      });
  });

  con(interval().duration, &Scenario::IntervalDurations::maxDurationChanged, this,
      [&](TimeVal sp) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([t = ctx.time(sp), cst = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        cst->set_max_duration(t);
      });
  });

  con(interval(), &Scenario::IntervalModel::mutedChanged, this, [&](bool b) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([b, itv = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        itv->mute(b);
      });
  });

  con(interval(), &Scenario::IntervalModel::busChanged, this, [&](bool b) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([b, itv = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        auto& audio_out
            = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.has_gain = b;
      });
  });
  con(*interval().outlet, &Process::AudioOutlet::gainChanged, this, [&](double g) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([g, itv = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        auto& audio_out
            = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.gain = g;
      });
  });
  con(*interval().outlet, &Process::AudioOutlet::panChanged, this,
      [&](ossia::pan_weight pan) {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    if(m_ossia_interval)
      in_exec([pan = std::move(pan), itv = m_ossia_interval] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
        auto& audio_out
            = static_cast<ossia::nodes::interval*>(itv->node.get())->audio_out;
        audio_out.pan = pan;
      });
  });

  if(scenar)
  {
    con(*interval().outlet, &Process::AudioOutlet::propagateChanged, this,
        [&, scenar](bool propag) {
      OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
      if(m_ossia_interval)
      {
        std::weak_ptr<ossia::graph_interface> g = this->system().execGraph;
        if(propag)
        {
          in_exec([g, cst = m_ossia_interval, scenar] {
            OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
            if(auto graph = g.lock())
            {
              auto& cables = cst->node->root_outputs()[0]->cables();
              SCORE_ASSERT(cables.size() == 0);
              auto cable = graph->allocate_edge(
                  ossia::immediate_glutton_connection{}, cst->node->root_outputs()[0],
                  scenar->node->root_inputs()[0], cst->node, scenar->node);
              graph->connect(cable);
            }
          });
        }
        else
        {
          in_exec([g, cst = m_ossia_interval] {
            OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
            if(auto graph = g.lock())
            {
              auto& cables = cst->node->root_outputs()[0]->cables();
              SCORE_ASSERT(cables.size() == 1);
              graph->disconnect(cables.front());
            }
          });
        }
      }
    });
  }
  // TODO tempo, etc
}

IntervalComponent::~IntervalComponent()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
}

void IntervalComponent::init()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(m_interval)
  {
    if(interval().muted())
    {
      m_ossia_interval->mute(true);
    }
    auto procs = interval().processes.size();
    if(procs > 0)
    {
      int safe_procs = 2 * (procs + 16);
      this->m_processes.reserve(safe_procs);
      m_ossia_interval->reserve_processes(safe_procs);

      int audio_outs = 0;
      for(auto& p : interval().processes)
      {
        auto& outs = p.outlets();
        for(auto& o : outs)
        {
          if(o->type() == Process::PortType::Audio)
            audio_outs++;
        }
      }

      const int safe_ins = 2 * (audio_outs + 1);
      ((ossia::nodes::forward_node*)m_ossia_interval->node.get())
          ->audio_in.sources.reserve(safe_ins);
    }
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
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  // itv has to be kept alive until end of this function
  if(auto itv = m_ossia_interval)
  {
    // self has to be kept alive until next tick
    in_exec([itv = itv, self = self, gcq_ptr = weak_gc] {
      OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
      itv->set_callback(ossia::time_interval::exec_callback{});
      itv->cleanup();

      // And then we want it to be cleared in the main thread
      if(auto gcq = gcq_ptr.lock())
        gcq->enqueue(gc(std::move(self), std::move(itv)));
    });

    if(m_interval)
    {
      auto [inputsToRegister, outputsToRegister] = portsToRegister(*m_interval);
      system().setup.unregister_node(
          inputsToRegister, outputsToRegister, m_ossia_interval->node);
    }
    else
    {
      qDebug() << "IntervalComponent::cleanup() ! interval has already been deleted";
    }
  }
  for(auto& proc : m_processes)
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
  if(interval().graphal())
    return {0_tv, 0_tv, ossia::Infinite, 1.};
  else
    return {
        context().time(interval().duration.defaultDuration()),
        context().time(interval().duration.minDuration()),
        context().time(interval().duration.maxDuration()), interval().duration.speed()};
}

void IntervalComponent::onSetup(
    std::shared_ptr<IntervalComponent> self,
    std::shared_ptr<ossia::time_interval> ossia_cst, interval_duration_data dur,
    bool root)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  m_ossia_interval = ossia_cst;
  Transaction t{context()};

#if defined(OSSIA_EXECUTION_LOG)
  m_ossia_interval->name = this->interval().metadata().getName().toStdString();
#endif

  if(!interval().graphal())
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
    m_ossia_interval->set_tempo_curve(std::move(tdata).first);

    // We always set the time signature at the topmost interval in order to not have things explode
    if(interval().hasTimeSignature() || root)
    {
      auto map = timeSignatureMap(interval(), context());
      if(map.empty())
        map[TimeVal::zero()] = ossia::time_signature{4, 4};
      m_ossia_interval->set_time_signature_map(std::move(map));
    }
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

  if(context().doc.app.applicationSettings.gui)
  {
    std::weak_ptr<IntervalComponent> weak_self = self;

    if(Q_UNLIKELY(interval().graphal()))
    {
      t.push_back([weak_self, ossia_cst, qed_ptr = weak_edit] {
        ossia_cst->set_callback(
            smallfun::function<void(bool, ossia::time_value), 32>{
                [weak_self, qed_ptr](bool running, ossia::time_value date) {
          OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
          if(auto qed = qed_ptr.lock())
            qed->enqueue([weak_self, running, date] {
              OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
              if(auto self = weak_self.lock())
                self->graph_slot_callback(running, date);
            });
        }});
      });
    }
    else
    {
      t.push_back([weak_self, ossia_cst, qed_ptr = weak_edit] {
        ossia_cst->set_callback(
            smallfun::function<void(bool, ossia::time_value), 32>{
                [weak_self, qed_ptr](bool running, ossia::time_value date) {
          OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
          if(auto qed = qed_ptr.lock())
            qed->enqueue([weak_self, running, date] {
              OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
              if(auto self = weak_self.lock())
                self->slot_callback(running, date);
            });
        }});
      });
    }
  }

  // set-up the interval ports
  {

    auto [inputsToRegister, outputsToRegister] = portsToRegister(interval());
    if(!inputsToRegister.empty() || !outputsToRegister.empty())
    {
      system().setup.register_node(
          inputsToRegister, outputsToRegister, m_ossia_interval->node, t);
    }
  }
  t.run_all();

  init();
}

void IntervalComponent::graph_slot_callback(bool running, ossia::time_value date)
{
  interval().setExecuting(running);
}

void IntervalComponent::slot_callback(bool running, ossia::time_value date)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(!m_interval)
    return;

  auto& cstdur = interval().duration;
  if(m_ossia_interval)
  {
    if(running)
    {
      const auto& maxdur = cstdur.maxDuration();

      auto currentTime = this->context().reverseTime(date);
      if(!maxdur.infinite())
      {
        if(maxdur > TimeVal::zero())
          cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
      }
      else
      {
        if(cstdur.defaultDuration() > TimeVal::zero())
          cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
      }
    }
    interval().setExecuting(running);
  }
  else
  {
    interval().setExecuting(false);
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
  interval().executionEvent(Scenario::IntervalExecutionEvent::Paused);
}

void IntervalComponentBase::resume()
{
  m_ossia_interval->resume();
  interval().executionEvent(Scenario::IntervalExecutionEvent::Resumed);
}

void IntervalComponentBase::stop()
{
  in_exec([cstr = m_ossia_interval] { cstr->stop(); });

  for(auto& process : m_processes)
  {
    process.second->stop();
  }
  interval().reset();

  executionStopped();
}

void IntervalComponentBase::executionStarted()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  interval().duration.setPlayPercentage(0);
  interval().executionEvent(Scenario::IntervalExecutionEvent::Playing);
  for(Process::ProcessModel& proc : interval().processes)
  {
    proc.setExecuting(true);
    proc.startExecution();
    proc.benchmark(-1.);
  }
}

void IntervalComponentBase::executionStopped()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(m_interval)
  {
    interval().executionEvent(Scenario::IntervalExecutionEvent::Stopped);
    for(Process::ProcessModel& proc : interval().processes)
    {
      proc.setExecuting(false);
      proc.stopExecution();
      proc.benchmark(-1.);
    }
  }
}

ProcessComponent*
IntervalComponentBase::make(ProcessComponentFactory& fac, Process::ProcessModel& proc)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  SCORE_ASSERT(this->OSSIAInterval());
  SCORE_ASSERT(this->OSSIAInterval()->node);
  try
  {
    const Execution::Context& ctx = system();
    auto plug = fac.make(proc, ctx, nullptr);
    if(plug && plug->OSSIAProcessPtr())
    {
      const auto& oproc = plug->OSSIAProcessPtr();
      SCORE_ASSERT(m_processes.find(proc.id()) == m_processes.end());
      m_processes.emplace(proc.id(), plug);
      SCORE_ASSERT(m_processes[proc.id()] == plug);

      // Node registration
      if(auto& onode = plug->node)
        ctx.setup.register_node(proc, onode);

      // Selection
      QObject::connect(
          &proc.selection, &Selectable::changed, plug.get(),
          [qex_ptr = weak_exec, n = oproc->node](bool ok) {
        if(auto qex = qex_ptr.lock())
          qex->enqueue([n, ok] {
            if(n)
              n->set_logging(ok);
          });
      });

      // Looping
      oproc->set_loops(proc.loops());
      con(proc, &Process::ProcessModel::loopsChanged, this,
          [qex_ptr = weak_exec, p = oproc](bool b) {
        if(auto qex = qex_ptr.lock())
          qex->enqueue([p, b] { p->set_loops(b); });
      });

      oproc->set_loop_duration(system().time(proc.loopDuration()));
      con(proc, &Process::ProcessModel::loopDurationChanged, this,
          [ctx_ptr = system().weakSelf(), p = oproc](TimeVal t) {
        if(auto ctx = ctx_ptr.lock())
          ctx->executionQueue.enqueue(
              [p, t = ctx->time(t)] { p->set_loop_duration(t); });
      });

      oproc->set_start_offset(system().time(proc.startOffset()));
      con(proc, &Process::ProcessModel::startOffsetChanged, this,
          [ctx_ptr = system().weakSelf(), p = oproc](TimeVal t) {
        if(auto ctx = ctx_ptr.lock())
          ctx->executionQueue.enqueue([p, t = ctx->time(t)] { p->set_start_offset(t); });
      });

      // Audio propagation
      auto reconnectOutlets = ReconnectOutlets<IntervalComponentBase>{
          *this, this->OSSIAInterval()->node, proc, oproc, system().execGraph};

      con(proc, &Process::ProcessModel::outletsChanged, this, reconnectOutlets);
      reconnectOutlets();

      // Logging
      if(oproc->node)
        oproc->node->set_logging(proc.selection.get());

      in_exec(AddProcess{
          m_ossia_interval, m_ossia_interval.get(), oproc, system().execGraph,
          propagatedOutlets(proc.outlets())});

      connect(
          plug.get(), &ProcessComponent::nodeChanged, this,
          HandleNodeChange{m_ossia_interval->node, oproc, system().execGraph, proc});
      return plug.get();
    }
  }
  catch(const std::exception& e)
  {
    qDebug() << "Error while creating a process: " << e.what();
  }
  catch(...)
  {
    qDebug() << "Error while creating a process";
  }

  qDebug() << "Could not create process for " << proc.metaObject()->className();
  return nullptr;
}

std::function<void()>
IntervalComponentBase::removing(const Process::ProcessModel& e, ProcessComponent& c)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  auto it = m_processes.find(e.id());
  if(it != m_processes.end())
  {
    if(m_ossia_interval)
    {
      in_exec([proc_ptr = std::weak_ptr{it->second}, cstr = m_ossia_interval,
               proc = c.OSSIAProcessPtr(), gcq_ptr = weak_gc]() mutable {
        cstr->remove_time_process(proc.get());

        if(auto cc = proc_ptr.lock())
          if(auto gcq = gcq_ptr.lock())
            gcq->enqueue(gc(std::move(cc), std::move(proc)));
      });
    }
    c.cleanup();

    return [this, cptr = std::weak_ptr{c.shared_from_this()}, id = e.id()] {
      m_processes.erase(id);
    };
  }
  return {};
}
}
