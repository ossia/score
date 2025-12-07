// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ControlMessage.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/nodes/state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_event.hpp>

#include <QDebug>

namespace Execution
{
namespace
{

std::vector<ossia::control_message>
toOssiaControls(const SetupContext& ctx, const Scenario::StateModel& state)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  const auto& msgs = state.controlMessages().messages();
  std::vector<ossia::control_message> ossia_msgs;
  ossia_msgs.reserve(msgs.size());
  for(const Process::ControlMessage& msg : msgs)
  {
    auto port = msg.port.try_find(ctx.context.doc);
    if(port)
    {
      auto it = ctx.inlets.find(port);
      if(it != ctx.inlets.end())
      {
        ossia::inlet& inlet = *it->second.second;
        if(ossia::value_port* port = inlet.target<ossia::value_port>())
          ossia_msgs.push_back(ossia::control_message{port, msg.value});
      }
    }
  }
  return ossia_msgs;
}
}
StateComponentBase::StateComponentBase(
    const Scenario::StateModel& element, std::shared_ptr<ossia::time_event> ev,
    const Execution::Context& ctx, QObject* parent)
    : Execution::Component{ctx, "Executor::State", nullptr}
    , m_model{&element}
    , m_ev{std::move(ev)}
    , m_node{ossia::make_node<ossia::nodes::state_writer>(
          *ctx.execState, Engine::score_to_ossia::state(element, *ctx.execState))}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  m_ev->add_time_process(std::make_shared<ossia::node_process>(m_node));

  system().setup.register_node({}, {}, m_node);

  connect(
      &element, &Scenario::StateModel::sig_statesUpdated, this,
      [this, st = std::weak_ptr{ctx.execState}] {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    if(auto dl = st.lock())
    {
      in_exec(
          [n = m_node, x = Engine::score_to_ossia::state(*m_model, *dl), dl]() mutable {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
        n->data = std::move(x);
      });
    }
  });

  connect(
      &element, &Scenario::StateModel::sig_controlMessagesUpdated, this,
      &StateComponentBase::updateControls);
  // Note : they aren't updated in the constructor, but in
  // DocumentPlugin::reload as the ports to which we're talking may not exist
  // yet at this time
}

void StateComponentBase::updateControls()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto ossia_msgs = toOssiaControls(this->system().setup, state());
  if(!ossia_msgs.empty())
  {
    in_exec([n = m_node, x = std::move(ossia_msgs)]() mutable {
      n->controls = std::move(x);
    });
  }
}

void StateComponentBase::onDelete() const
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  if(m_node)
  {
    system().setup.unregister_node({}, {}, m_node);
    if(m_ev)
    {
      auto foo = [ev = m_ev, gcq = weak_gc] { };
      in_exec([ev = m_ev, gcq = weak_gc] {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
        auto& procs = ev->get_time_processes();
        if(!procs.empty())
        {
          auto proc = (*procs.begin());
          ev->remove_time_process(proc.get());
          if(auto gc_queue = gcq.lock())
          {
            gc_queue->enqueue(gc(std::move(proc)));
          }
        }
      });
    }
  }
}

ProcessComponent*
StateComponentBase::make(ProcessComponentFactory& fac, Process::ProcessModel& proc)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  try
  {
    const Execution::Context& ctx = system();
    auto plug = fac.make(proc, ctx, nullptr);
    if(plug && plug->OSSIAProcessPtr())
    {
      auto oproc = plug->OSSIAProcessPtr();
      m_processes.emplace(proc.id(), plug);

      if(auto& onode = plug->node)
        ctx.setup.register_node(proc, onode);

      auto cst = m_ev;

      QObject::connect(
          &proc.selection, &Selectable::changed, plug.get(),
          [this, n = oproc->node](bool ok) {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
        in_exec([n, ok] {
          OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
          if(n)
            n->set_logging(ok);
        });
      });
      if(oproc->node)
        oproc->node->set_logging(proc.selection.get());

      std::weak_ptr<ossia::time_process> oproc_weak = oproc;
      std::weak_ptr<ossia::graph_interface> g_weak = plug->system().execGraph;

      in_exec([cst = m_ev, oproc_weak, g_weak] {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
        if(auto oproc = oproc_weak.lock())
          if(auto g = g_weak.lock())
          {
            cst->add_time_process(oproc);
          }
      });

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
  return nullptr;
}

std::function<void()>
StateComponentBase::removing(const Process::ProcessModel& e, ProcessComponent& c)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto it = m_processes.find(e.id());
  if(it != m_processes.end())
  {
    if(m_ev)
    {
      if(auto time_process = c.OSSIAProcessPtr())
      {
        in_exec([process_component_ptr = std::weak_ptr{it->second}, cstr = m_ev,
                 time_process = std::move(time_process), gcq_ptr = weak_gc]() mutable {
          OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
          cstr->remove_time_process(time_process.get());

          if(auto cc = process_component_ptr.lock())
            if(auto gcq = gcq_ptr.lock())
              gcq->enqueue(gc(std::move(cc), std::move(time_process)));
        });
      }
    }

    c.cleanup();

    return [this, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      m_processes.erase(id);
    };
  }

  return {};
}

StateComponent::~StateComponent() { }

void StateComponent::init()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  if(m_model)
  {
    init_hierarchy();
  }
}

void StateComponent::cleanup(const std::shared_ptr<StateComponent>& self)
{
  // FIXME this does not match IntervalComponent
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  if(m_ev)
  {
    // self has to be kept alive until next tick
    in_exec([self, gcq_ptr = weak_gc] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      if(auto gcq = gcq_ptr.lock())
        gcq->enqueue(gc(self));
    });
  }

  onDelete();

  for(auto& [key, c] : m_processes)
  {
    SCORE_ASSERT(c);
    if(auto time_process = c->OSSIAProcessPtr())
    {
      in_exec([process_component_ptr = std::weak_ptr{c}, cstr = m_ev,
               time_process = std::move(time_process), gcq_ptr = weak_gc]() mutable {
        cstr->remove_time_process(time_process.get());

        if(auto cc = process_component_ptr.lock())
          if(auto gcq = gcq_ptr.lock())
            gcq->enqueue(gc(std::move(cc), std::move(time_process)));
      });
    }
    c->cleanup();
  }

  clear();
  m_processes.clear();
  m_ev.reset();
  m_node.reset();
  disconnect();
}
}
