#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/PortForward.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/tools/std/HashMap.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QMetaObject>

#include <nano_observer.hpp>

namespace ossia
{
class time_process;
}

namespace Process
{
class ProcessModel;
class ProcessModel;
class Cable;
}

namespace State
{
struct AddressAccessor;
}

namespace Execution
{
struct Transaction;
struct SCORE_LIB_PROCESS_EXPORT SetupContext final
    : public QObject
    , public Nano::Observer
{
  explicit SetupContext(Context& other) noexcept;
  ~SetupContext();
  Context& context;
  void register_node(
      const Process::ProcessModel& proc, const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node(
      const Process::ProcessModel& proc, const std::shared_ptr<ossia::graph_node>& node);
  void register_node(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node_soft(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node, Transaction& vec);

  void set_destination(const State::AddressAccessor& address, const ossia::inlet_ptr&);
  void set_destination(const State::AddressAccessor& address, const ossia::outlet_ptr&);

  void register_inlet(
      Process::Inlet& inlet, const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node);
  void register_outlet(
      Process::Outlet& outlet, const ossia::outlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node);

  void unregister_inlet(
      const Process::Inlet& inlet, const std::shared_ptr<ossia::graph_node>& node);
  void unregister_outlet(
      const Process::Outlet& outlet, const std::shared_ptr<ossia::graph_node>& node);

  // Deferred versions, stored in a vec
  void register_node(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node, Transaction& vec);
  void unregister_node(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node, Transaction& vec);
  void register_node(
      const Process::ProcessModel& proc, const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);
  void unregister_node(
      const Process::ProcessModel& proc, const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);

  void register_inlet(
      Process::Inlet& inlet, const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node, Transaction& vec);
  void register_outlet(
      Process::Outlet& outlet, const ossia::outlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node, Transaction& vec);

  void unregister_inlet(
      const Process::Inlet& inlet, const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);
  void unregister_outlet(
      const Process::Outlet& outlet, const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);

  void replace_node(
      const std::shared_ptr<ossia::time_process>& process,
      const std::shared_ptr<ossia::graph_node>& node, Transaction& commands);

  void on_cableCreated(Process::Cable& c);
  void on_cableRemoved(const Process::Cable& c);
  void connectCable(Process::Cable& cable);
  void connectCable(Process::Cable& c, Transaction& vec);
  void removeCable(const Process::Cable& c);
  void removeCable(const Process::Cable& c, Transaction& vec);

  score::hash_map<Process::Outlet*, std::pair<ossia::node_ptr, ossia::outlet_ptr>>
      outlets;
  score::hash_map<Process::Inlet*, std::pair<ossia::node_ptr, ossia::inlet_ptr>> inlets;
  score::hash_map<Id<Process::Cable>, std::shared_ptr<ossia::graph_edge>> m_cables;

  struct RegisteredPorts
  {
    ossia::flat_map<Id<Process::Port>, QMetaObject::Connection> inlets;
    ossia::flat_map<Id<Process::Port>, QMetaObject::Connection> outlets;
    void clear() const noexcept
    {
      for(auto& con : inlets)
        QObject::disconnect(con.second);
      for(auto& con : outlets)
        QObject::disconnect(con.second);
    }
  };

  score::hash_map<std::shared_ptr<ossia::graph_node>, RegisteredPorts>
      runtime_connections;
  score::hash_map<const ossia::graph_node*, const Process::ProcessModel*> proc_map;

private:
  template <typename Impl>
  void register_node_impl(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node, Impl&&);

  template <typename Impl>
  void register_inlet_impl(
      Process::Inlet& inlet, const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node, Impl&&);

  template <typename Impl>
  void register_outlet_impl(
      Process::Outlet& outlet, const ossia::outlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node, Impl&&);

  template <typename Impl>
  void connect_cable_impl(Process::Cable& c, Impl&&);

  template <typename Impl>
  void disconnect_cable_impl(const Process::Cable& c, Impl&&);
};
}
