#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionFunctions.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Process.hpp>
#include <State/Address.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>

namespace Execution
{
ossia::net::node_base*
findNode(const ossia::execution_state& st, const State::Address& addr)
{
  auto& devs = st.edit_devices();
  auto dev_p
      = ossia::find_if(devs, [d = addr.device.toStdString()](auto& dev) {
          return dev->get_name() == d;
        });
  if (dev_p == devs.end())
    return nullptr;
  return ossia::net::find_node(
      (*dev_p)->get_root_node(), addr.path.join("/").toStdString());
}

optional<ossia::destination> makeDestination(
    const ossia::execution_state& devices,
    const State::AddressAccessor& addr)
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
        [cable = it->second, graph = context.execGraph] {
          graph->disconnect(cable);
        });
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
    context.executionQueue.enqueue([edge, graph = context.execGraph] {
      graph->connect(std::move(edge));
    });
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
    std::vector<ExecutionCommand>& vec)
{
  register_node(proc.inlets(), proc.outlets(), node, vec);
  proc_map[node.get()] = &proc;
}

void SetupContext::unregister_node(
    const Process::ProcessModel& proc,
    const std::shared_ptr<ossia::graph_node>& node,
    std::vector<ExecutionCommand>& vec)
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
  if(!g)
    return;

  if (address.address.device.isEmpty())
  {
    append([=] {
      if(port->address)
      {
        s->unregister_port(*port);
        port->address = {};
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
        port->address = p;
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
        port->address = n;
        s->register_port(*port);
        g->mark_dirty();
      });
    }
  }
  else if(auto ad = address.address.toString_unsafe().toStdString();
          ossia::traversal::is_pattern(ad))
  {
    // OPTIMIZEME
    auto path = ossia::traversal::make_path(ad);
    if (path)
    {
      append([=, p = *path]() mutable {
        s->unregister_port(*port);
        port->address = std::move(p);
        if (ossia::value_port* dat
            = port->template target<ossia::value_port>())
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
        port->address = {};
        if (ossia::value_port* dat
            = port->template target<ossia::value_port>())
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
  set_destination_impl(context, address, port, [this](auto&& f) {
    context.executionQueue.enqueue(std::move(f));
  });
}

void SetupContext::set_destination(
    const State::AddressAccessor& address,
    const ossia::outlet_ptr& port)
{
  set_destination_impl(context, address, port, [this](auto&& f) {
    context.executionQueue.enqueue(std::move(f));
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

    const std::size_t ossia_n_inlets = node->inputs().size();
    const std::size_t ossia_n_outlets = node->outputs().size();

    std::size_t n_inlets = std::min(proc_n_inlets, ossia_n_inlets);
    std::size_t n_outlets = std::min(proc_n_outlets, ossia_n_outlets);

    auto& runtime_connection = runtime_connections[node];

    for (std::size_t i = 0; i < n_inlets; i++)
    {
      Process::Inlet* score_port = proc_inlets[i];
      runtime_connection[score_port->id()] = connect(
          proc_inlets[i],
          &Process::Port::addressChanged,
          this,
          [this,
           port = node->inputs()[i]](const State::AddressAccessor& address) {
            set_destination(address, port);
          });
      SCORE_ASSERT(node->inputs()[i]);
      set_destination_impl(
          context, score_port->address(), node->inputs()[i], exec);

      inlets.insert({score_port, std::make_pair(node, node->inputs()[i])});

      exec([this, port = node->inputs()[i]] {
        context.execState->register_port(*port);
      });
    }

    for (std::size_t i = 0; i < n_outlets; i++)
    {
      Process::Outlet* score_port = proc_outlets[i];
      runtime_connection[score_port->id()] = connect(
          score_port,
          &Process::Port::addressChanged,
          this,
          [this,
           port = node->outputs()[i]](const State::AddressAccessor& address) {
            set_destination(address, port);
          });
      SCORE_ASSERT(node->outputs()[i]);
      set_destination_impl(
          context, score_port->address(), node->outputs()[i], exec);

      outlets.insert(
          {score_port, std::make_pair(node, node->outputs()[i])});
    }
  }
}

void SetupContext::register_node(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node)
{
  register_node_impl(proc_inlets, proc_outlets, node, [this](auto&& f) {
    context.executionQueue.enqueue(std::move(f));
  });
}

void SetupContext::register_node(
    const Process::Inlets& proc_inlets,
    const Process::Outlets& proc_outlets,
    const std::shared_ptr<ossia::graph_node>& node,
    std::vector<ExecutionCommand>& vec)
{
  register_node_impl(proc_inlets, proc_outlets, node, [&](auto&& f) {
    vec.push_back(std::move(f));
  });
}

void SetupContext::register_inlet(
    Process::Inlet& proc_port,
    const ossia::inlet_ptr& ossia_port,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    SCORE_ASSERT(ossia_port);
    auto& con = runtime_connection[proc_port.id()];
    QObject::disconnect(con);
    con = connect(
          &proc_port,
          &Process::Port::addressChanged,
          this,
          [this, ossia_port](const State::AddressAccessor& address) {
      set_destination(address, ossia_port);
    });
    set_destination(proc_port.address(), ossia_port);

    inlets.insert({&proc_port, std::make_pair(node, ossia_port)});

    std::weak_ptr<ossia::execution_state> ws = context.execState;
    context.executionQueue.enqueue([ws, ossia_port] {
      if (auto state = ws.lock())
        state->register_port(*ossia_port);
    });
  }
}

void SetupContext::register_outlet(
    Process::Outlet& proc_port,
    const ossia::outlet_ptr& ossia_port,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    SCORE_ASSERT(ossia_port);
    auto& con = runtime_connection[proc_port.id()];
    QObject::disconnect(con);
    con = connect(
          &proc_port,
          &Process::Port::addressChanged,
          this,
          [this, ossia_port](const State::AddressAccessor& address) {
      set_destination(address, ossia_port);
    });
    set_destination(proc_port.address(), ossia_port);

    outlets.insert({&proc_port, std::make_pair(node, ossia_port)});

    // Unneeded : the execution_state only needs inlets to be registered,
    // in order to set up data value queues from the network thread

    // std::weak_ptr<ossia::execution_state> ws = context.execState;
    // context.executionQueue.enqueue([ws, ossia_port] {
    //   if (auto state = ws.lock())
    //     state->register_outlet(*ossia_port);
    // });
  }
}

void SetupContext::unregister_inlet(
    const Process::Inlet& proc_port,
    const std::shared_ptr<ossia::graph_node>& node)
{
  if (node)
  {
    auto& runtime_connection = runtime_connections[node];
    auto it = runtime_connection.find(proc_port.id());
    if(it != runtime_connection.end())
    {
      QObject::disconnect(it->second);
      runtime_connection.erase(it);
    }

    auto ossia_port_it = inlets.find(const_cast<Process::Inlet*>(&proc_port));
    if(ossia_port_it != inlets.end())
    {
      std::weak_ptr<ossia::execution_state> ws = context.execState;
      context.executionQueue.enqueue([ws, ossia_port=ossia_port_it.value().second] {
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
    if(it != runtime_connection.end())
    {
      QObject::disconnect(it->second);
      runtime_connection.erase(it);
    }
  }

  outlets.erase(const_cast<Process::Outlet*>(&proc_port));
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
    std::vector<ExecutionCommand>& vec)
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
