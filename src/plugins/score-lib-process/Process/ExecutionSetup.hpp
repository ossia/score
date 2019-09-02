#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/tools/std/HashMap.hpp>

#include <ossia/detail/small_vector.hpp>

#include <QMetaObject>
namespace Process
{
class ProcessModel;
class ProcessModel;
class Inlet;
class Outlet;
class Cable;
using Inlets = ossia::small_vector<Process::Inlet*, 4>;
using Outlets = ossia::small_vector<Process::Outlet*, 4>;
}

namespace State
{
struct AddressAccessor;
}

namespace Execution
{
struct Context;
struct SCORE_LIB_PROCESS_EXPORT SetupContext final : public QObject
{
  SetupContext(Context& other) : context{other} {}
  Context& context;
  void register_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node);
  void register_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node_soft(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);

  void set_destination(
      const State::AddressAccessor& address,
      const ossia::inlet_ptr&);
  void set_destination(
      const State::AddressAccessor& address,
      const ossia::outlet_ptr&);

  void register_inlet(
      Process::Inlet& inlet,
      const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node);
  void register_outlet(
      Process::Outlet& outlet,
      const ossia::outlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_inlet(
      const Process::Inlet& inlet,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_outlet(
      const Process::Outlet& outlet,
      const std::shared_ptr<ossia::graph_node>& node);

  // Deferred versions, stored in a vec
  void register_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node,
      std::vector<ExecutionCommand>& vec);
  void unregister_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node,
      std::vector<ExecutionCommand>& vec);
  void register_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node,
      std::vector<ExecutionCommand>& vec);
  void unregister_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node,
      std::vector<ExecutionCommand>& vec);

  void on_cableCreated(Process::Cable& c);
  void on_cableRemoved(const Process::Cable& c);
  void connectCable(Process::Cable& cable);

  score::
      hash_map<Process::Outlet*, std::pair<ossia::node_ptr, ossia::outlet_ptr>>
          outlets;
  score::
      hash_map<Process::Inlet*, std::pair<ossia::node_ptr, ossia::inlet_ptr>>
          inlets;
  score::hash_map<Id<Process::Cable>, std::shared_ptr<ossia::graph_edge>>
      m_cables;

  score::hash_map<
      std::shared_ptr<ossia::graph_node>,
      score::hash_map<Id<Process::Port>, QMetaObject::Connection>
  >
      runtime_connections;
  score::hash_map<const ossia::graph_node*, const Process::ProcessModel*>
      proc_map;

private:
  template <typename Impl>
  void register_node_impl(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node,
      Impl&&);
};
}
