#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionFunctions.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Process.hpp>
#include <State/Address.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/editor/scenario/time_process.hpp>

namespace Execution
{

static auto enqueue_in_context(SetupContext& self) noexcept
{
  return [&self](auto&& f) { self.context.executionQueue.enqueue(std::move(f)); };
}

static auto enqueue_in_vector(Transaction& vec) noexcept
{
  return [&vec](auto&& f) { vec.push_back(std::move(f)); };
}

ossia::net::node_base* findNode(const ossia::execution_state& st, const State::Address& addr)
{
  auto& devs = st.edit_devices();
  auto dev_p = ossia::find_if(
      devs, [d = addr.device.toStdString()](auto& dev) { return dev->get_name() == d; });
  if (dev_p == devs.end())
    return nullptr;
  return ossia::net::find_node((*dev_p)->get_root_node(), addr.path.join("/").toStdString());
}

std::optional<ossia::destination>
makeDestination(const ossia::execution_state& devices, const State::AddressAccessor& addr)
{
  auto n = findNode(devices, addr.address);
  if (!n)
    return {};

  auto p = n->get_parameter();
  if (!p)
    return {};

  auto& qual = addr.qualifiers.get();
  return ossia::destination{*p, qual.accessors, qual.unit};
}

void SetupContext::on_cableCreated(Process::Cable& c)
{
  connectCable(c);
}

void SetupContext::on_cableRemoved(const Process::Cable& c)
{
  if (!context.created)
    return;
  auto it = m_cables.find(c.id());
  if (it != m_cables.end())
  {
    context.executionQueue.enqueue(
        [cable = it->second, graph = context.execGraph] { graph->disconnect(cable); });
  }
}

void SetupContext::connectCable(Process::Cable& cable)
{
  if (!context.created)
    return;
  ossia::node_ptr source_node{}, sink_node{};
  ossia::outlet_ptr source_port{};
  ossia::inlet_ptr sink_port{};
  if (auto port_src = cable.source().try_find(context.doc))
  {
    auto it = outlets.find(port_src);
    if (it != outlets.end())
    {
      source_node = it->second.first;
      source_port = it->second.second;
    }
  }
  if (auto port_snk = cable.sink().try_find(context.doc))
  {
    auto it = inlets.find(port_snk);
    if (it != inlets.end())
    {
      sink_node = it->second.first;
      sink_port = it->second.second;
    }
  }

  if (source_node && sink_node && source_port && sink_port)
  {
    ossia::edge_ptr edge;
    switch (cable.type())
    {
      case Process::CableType::ImmediateStrict:
      {
        edge = ossia::make_edge(
            ossia::immediate_strict_connection{},
            std::move(source_port),
            std::move(sink_port),
            std::move(source_node),
            std::move(sink_node));
        break;
      }
      case Process::CableType::ImmediateGlutton:
      {
        edge = ossia::make_edge(
            ossia::immediate_glutton_connection{},
            std::move(source_port),
            std::move(sink_port),
            std::move(source_node),
            std::move(sink_node));
        break;
      }
      case Process::CableType::DelayedStrict:
      {
        edge = ossia::make_edge(
            ossia::delayed_strict_connection{},
            std::move(source_port),
            std::move(sink_port),
            std::move(source_node),
            std::move(sink_node));
        break;
      }
      case Process::CableType::DelayedGlutton:
      {
        edge = ossia::make_edge(
            ossia::delayed_glutton_connection{},
            std::move(source_port),
            std::move(sink_port),
            std::move(source_node),
            std::move(sink_node));
        break;
      }
    }

    m_cables[cable.id()] = edge;
    context.executionQueue.enqueue(
        [edge, graph = context.execGraph]() mutable { graph->connect(std::move(edge)); });
  }
}

template <typename Impl>
void SetupContext::register_inlet_impl(
    Process::Inlet& proc_port,
    const ossia::inlet_ptr& ossia_port,
    const std::shared_ptr<ossia::graph_node>& node,
    Impl&& impl)
{
  SCORE_ASSERT(node);
  SCORE_ASSERT(ossia_port);

  auto& runtime_connection = runtime_connections[node];
  auto& con = runtime_connection[proc_port.id()];
  QObject::disconnect(con);
  con = connect(
      &proc_port,
      &Process::Port::addressChanged,
      this,
      [this, ossia_port](const State::AddressAccessor& address) {
        set_destination(address, ossia_port);
      });
  set_destination_impl(context, proc_port.address(), ossia_port, impl);

  inlets.insert({&proc_port, std::make_pair(node, ossia_port)});

  std::weak_ptr<ossia::execution_state> ws = context.execState;
  context.executionQueue.enqueue([ws, ossia_port] {
    if (auto state = ws.lock())
      state->register_port(*ossia_port);
  });
}

template <typename Impl>
void SetupContext::register_node_impl(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node,
    Impl&& exec)
{
  if (node)
  {
    std::weak_ptr<ossia::graph_interface> wg = context.execGraph;
    exec([wg, node]() mutable {
      if (auto g = wg.lock())
        g->add_node(std::move(node));
    });

    const std::size_t proc_n_inlets = proc_inlets.size();
    const std::size_t proc_n_outlets = proc_outlets.size();

    const std::size_t ossia_n_inlets = node->root_inputs().size();
    const std::size_t ossia_n_outlets = node->root_outputs().size();

    std::size_t n_inlets = std::min(proc_n_inlets, ossia_n_inlets);
    std::size_t n_outlets = std::min(proc_n_outlets, ossia_n_outlets);

    for (std::size_t i = 0; i < n_inlets; i++)
    {
      register_inlet_impl(*proc_inlets[i], node->root_inputs()[i], node, exec);
    }

    for (std::size_t i = 0; i < n_outlets; i++)
    {
      register_outlet_impl(*proc_outlets[i], node->root_outputs()[i], node, exec);
    }
  }
}

void SetupContext::register_node(
    const Process::ProcessModel& proc,
    const std::shared_ptr<ossia::graph_node>& node)
{
  register_node(proc.inlets(), proc.outlets(), node);
  proc_map[node.get()] = &proc;
}

void SetupContext::unregister_node(
    const Process::ProcessModel& proc,
    const std::shared_ptr<ossia::graph_node>& node)
{
  unregister_node(proc.inlets(), proc.outlets(), node);
  proc_map.erase(node.get());
}

void SetupContext::register_node(
    const Process::ProcessModel& proc,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& vec)
{
  register_node(proc.inlets(), proc.outlets(), node, vec);
  proc_map[node.get()] = &proc;
}

void SetupContext::unregister_node(
    const Process::ProcessModel& proc,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& vec)
{
  unregister_node(proc.inlets(), proc.outlets(), node, vec);
  proc_map.erase(node.get());
}

template <typename T, typename Impl>
void set_destination_impl(
    const Context& plug,
    const State::AddressAccessor& address,
    const T& port,
    Impl&& append)
{
  auto& s = plug.execState;
  auto& g = plug.execGraph;
  if (!g)
    return;

  if (address.address.device.isEmpty())
  {
    append([=] {
      for(auto& addr : port->addresses)
      {
        s->unregister_port(*port);
        addr = {};
        if (ossia::value_port* dat = port->template target<ossia::value_port>())
        {
          dat->type = {};
          dat->index = {};
        }
        g->mark_dirty();
      }
    });
    return;
  }

  auto& qual = address.qualifiers.get();
  if (auto n = findNode(*plug.execState, address.address))
  {
    auto p = n->get_parameter();
    if (p)
    {
      append([=] {
        s->unregister_port(*port);
        port->addresses = {p};
        if (ossia::value_port* dat = port->template target<ossia::value_port>())
        {
          if (qual.unit)
            dat->type = qual.unit;
          dat->index = qual.accessors;
        }
        s->register_port(*port);
        g->mark_dirty();
      });
    }
    else
    {
      append([=] {
        s->unregister_port(*port);
        port->addresses = {n};
        s->register_port(*port);
        g->mark_dirty();
      });
    }
  }
  else if (auto ad = address.address.toString_unsafe().toStdString();
           ossia::traversal::is_pattern(ad))
  {
    // OPTIMIZEME
    auto path = ossia::traversal::make_path(ad);
    if (path)
    {
      append([=, p = *path]() mutable {
        s->unregister_port(*port);
        port->addresses = {std::move(p)};
        if (ossia::value_port* dat = port->template target<ossia::value_port>())
        {
          dat->type = {};
          dat->index.clear();
        }
        s->register_port(*port);
        g->mark_dirty();
      });
    }
    else
    {
      append([=] {
        s->unregister_port(*port);
        port->addresses.clear();
        if (ossia::value_port* dat = port->template target<ossia::value_port>())
        {
          dat->type = {};
          dat->index.clear();
        }
        s->register_port(*port);
        g->mark_dirty();
      });
    }
  }
}

void SetupContext::set_destination(
    const State::AddressAccessor& address,
    const ossia::inlet_ptr& port)
{
  set_destination_impl(context, address, port, enqueue_in_context(*this));
}

void SetupContext::set_destination(
    const State::AddressAccessor& address,
    const ossia::outlet_ptr& port)
{
  set_destination_impl(context, address, port, enqueue_in_context(*this));
}

void SetupContext::register_inlet(
    Process::Inlet& inlet,
    const ossia::inlet_ptr& exec,
    const std::shared_ptr<ossia::graph_node>& node)
{
  register_inlet_impl(inlet, exec, node, enqueue_in_context(*this));
}
void SetupContext::register_outlet(
    Process::Outlet& outlet,
    const ossia::outlet_ptr& exec,
    const std::shared_ptr<ossia::graph_node>& node)
{
  register_outlet_impl(outlet, exec, node, enqueue_in_context(*this));
}

void SetupContext::register_node(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node)
{
  register_node_impl(proc_inlets, proc_outlets, node, enqueue_in_context(*this));
}

void SetupContext::register_node(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& vec)
{
  register_node_impl(proc_inlets, proc_outlets, node, enqueue_in_vector(vec));
}

void SetupContext::register_inlet(
    Process::Inlet& inlet,
    const ossia::inlet_ptr& exec,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& vec)
{
  register_inlet_impl(inlet, exec, node, enqueue_in_vector(vec));
}
void SetupContext::register_outlet(
    Process::Outlet& outlet,
    const ossia::outlet_ptr& exec,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& vec)
{
  register_outlet_impl(outlet, exec, node, enqueue_in_vector(vec));
}

template <typename Impl>
void SetupContext::register_outlet_impl(
    Process::Outlet& proc_port,
    const ossia::outlet_ptr& ossia_port,
    const std::shared_ptr<ossia::graph_node>& node,
    Impl&& impl)
{
  SCORE_ASSERT(node);
  SCORE_ASSERT(ossia_port);
  auto& runtime_connection = runtime_connections[node];
  auto& con = runtime_connection[proc_port.id()];
  QObject::disconnect(con);
  con = connect(
      &proc_port,
      &Process::Port::addressChanged,
      this,
      [this, ossia_port](const State::AddressAccessor& address) {
        set_destination(address, ossia_port);
      });
  set_destination_impl(context, proc_port.address(), ossia_port, impl);

  outlets.insert({&proc_port, std::make_pair(node, ossia_port)});

  proc_port.mapExecution(*ossia_port, [&](Process::Inlet& model_inl, ossia::inlet& ossia_inl) {
    register_inlet_impl(model_inl, &ossia_inl, node, impl);
  });

  // Unneeded : the execution_state only needs inlets to be registered,
  // in order to set up data value queues from the network thread

  // std::weak_ptr<ossia::execution_state> ws = context.execState;
  // context.executionQueue.enqueue([ws, ossia_port] {
  //   if (auto state = ws.lock())
  //     state->register_outlet(*ossia_port);
  // });
}

void SetupContext::unregister_inlet(
    const Process::Inlet& proc_port,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    auto it = runtime_connection.find(proc_port.id());
    if (it != runtime_connection.end())
    {
      QObject::disconnect(it->second);
      runtime_connection.erase(it);
    }

    auto ossia_port_it = inlets.find(const_cast<Process::Inlet*>(&proc_port));
    if (ossia_port_it != inlets.end())
    {
      std::weak_ptr<ossia::execution_state> ws = context.execState;
      context.executionQueue.enqueue([ws, ossia_port = ossia_port_it.value().second] {
        if (auto state = ws.lock())
          state->unregister_port(*ossia_port);
      });

      inlets.erase(ossia_port_it);
    }
  }
  else
  {
    inlets.erase(const_cast<Process::Inlet*>(&proc_port));
  }
}

void SetupContext::unregister_outlet(
    const Process::Outlet& proc_port,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    auto it = runtime_connection.find(proc_port.id());
    if (it != runtime_connection.end())
    {
      QObject::disconnect(it->second);
      runtime_connection.erase(it);
    }

    proc_port.forChildInlets(
        [&](Process::Inlet& model_inl) { unregister_inlet(model_inl, node); });
  }

  outlets.erase(const_cast<Process::Outlet*>(&proc_port));
}

void SetupContext::unregister_inlet(
    const Process::Inlet& proc_port,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& commands)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    auto it = runtime_connection.find(proc_port.id());
    if (it != runtime_connection.end())
    {
      QObject::disconnect(it->second);
      runtime_connection.erase(it);
    }

    auto ossia_port_it = inlets.find(const_cast<Process::Inlet*>(&proc_port));
    if (ossia_port_it != inlets.end())
    {
      std::weak_ptr<ossia::execution_state> ws = context.execState;
      commands.push_back([ws, ossia_port = ossia_port_it.value().second] {
        if (auto state = ws.lock())
          state->unregister_port(*ossia_port);
      });

      inlets.erase(ossia_port_it);
    }
  }
  else
  {
    inlets.erase(const_cast<Process::Inlet*>(&proc_port));
  }
}

void SetupContext::unregister_outlet(
    const Process::Outlet& proc_port,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& commands)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    auto it = runtime_connection.find(proc_port.id());
    if (it != runtime_connection.end())
    {
      QObject::disconnect(it->second);
      runtime_connection.erase(it);
    }

    proc_port.forChildInlets(
        [&](Process::Inlet& model_inl) { unregister_inlet(model_inl, node); });
  }

  outlets.erase(const_cast<Process::Outlet*>(&proc_port));
}

void SetupContext::replace_node(
    const std::shared_ptr<ossia::time_process>& process,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& commands)
{
  commands.push_back([p=process,n=node] () mutable {
    using namespace std;
    swap(p->node, n);
  });
}

void SetupContext::unregister_node(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    std::weak_ptr<ossia::graph_interface> wg = context.execGraph;
    context.executionQueue.enqueue([wg, node] {
      if (auto g = wg.lock())
        g->remove_node(node);
      node->clear();
    });

    for (const auto& con : runtime_connections[node])
    {
      QObject::disconnect(con.second);
    }
    runtime_connections.erase(node);

    proc_map.erase(node.get());
  }

  for (auto ptr : proc_inlets)
    inlets.erase(ptr);
  for (auto ptr : proc_outlets)
    outlets.erase(ptr);
}

void SetupContext::unregister_node(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node,
    Transaction& vec)
{
  if (node)
  {
    std::weak_ptr<ossia::graph_interface> wg = context.execGraph;
    vec.push_back([wg, node] {
      if (auto g = wg.lock())
        g->remove_node(node);
      node->clear();
    });

    for (const auto& con : runtime_connections[node])
    {
      QObject::disconnect(con.second);
    }
    runtime_connections.erase(node);

    proc_map.erase(node.get());
  }

  for (auto ptr : proc_inlets)
    inlets.erase(ptr);
  for (auto ptr : proc_outlets)
    outlets.erase(ptr);
}

void SetupContext::unregister_node_soft(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    for (const auto& con : runtime_connections[node])
    {
      QObject::disconnect(con.second);
    }
    runtime_connections.erase(node);

    proc_map.erase(node.get());
  }

  for (auto ptr : proc_inlets)
    inlets.erase(ptr);
  for (auto ptr : proc_outlets)
    outlets.erase(ptr);
}
}
