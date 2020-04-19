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
template<typename T>
inline constexpr auto gc(T&& t) noexcept {
  return [gced = std::move(t)] { };
}

struct Transaction
{
  const Context& context;
  std::vector<ExecutionCommand> commands;
  Transaction(const Context& ctx) noexcept
    : context{ctx}
  {

  }

  Transaction(Transaction&& other) noexcept
    : context{other.context}
    , commands{std::move(other.commands)}
  {

  }

  Transaction& operator=(Transaction&& other) noexcept
  {
    commands = std::move(other.commands);
    return *this;
  }
  void reserve(std::size_t sz) noexcept { commands.reserve(sz); }
  bool empty() const noexcept { return commands.empty(); }
  template<typename T>
  void push_back(T&& t) noexcept {
    commands.push_back(std::move(t));
  }

  void run_all()
  {
    context.executionQueue.enqueue(
          [t=std::move(*this)] () mutable {
      t.run_all_in_exec();
    });
  }

  void run_all_in_exec()
  {
    for(auto& cmd : commands)
      cmd();

    context.editionQueue.enqueue(gc(std::move(*this)));
  }
};

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
      Transaction& vec);
  void unregister_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);
  void register_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);
  void unregister_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);

  void register_inlet(
      Process::Inlet& inlet,
      const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);
  void register_outlet(
      Process::Outlet& outlet,
      const ossia::outlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);

  void unregister_inlet(
      const Process::Inlet& inlet,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);
  void unregister_outlet(
      const Process::Outlet& outlet,
      const std::shared_ptr<ossia::graph_node>& node,
      Transaction& vec);

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

  template <typename Impl>
  void register_inlet_impl(
      Process::Inlet& inlet,
      const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node,
      Impl&&);

  template <typename Impl>
  void register_outlet_impl(
      Process::Outlet& outlet,
      const ossia::outlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node,
      Impl&&);

};
}
