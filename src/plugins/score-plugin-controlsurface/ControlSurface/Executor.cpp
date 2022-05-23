#include "Executor.hpp"
#include <Process/ExecutionFunctions.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
namespace ossia
{
template<typename T>
std::function<void(T&)> set_destination_impl(
    execution_state& s,
    graph_interface& g,
    const State::AddressAccessor& address)
{
  if (address.address.device.isEmpty())
  {
    return [&s, &g] (T& port) {
      if (port.address)
      {
        s.unregister_port(port);
        port.address = {};
        if (ossia::value_port* dat
            = port.template target<ossia::value_port>())
        {
          dat->type = {};
          dat->index = {};
        }
        g.mark_dirty();
      }
    };
  }

  auto& qual = address.qualifiers.get();
  if (auto n = Execution::findNode(s, address.address))
  {
    return [&s, &g, n, qual] (T& port) {
      auto p = n->get_parameter();
      if (p)
      {
        s.unregister_port(port);
        port.address = p;
        if (ossia::value_port* dat
            = port.template target<ossia::value_port>())
        {
          if (qual.unit)
            dat->type = qual.unit;
          dat->index = qual.accessors;
        }
        s.register_port(port);
        g.mark_dirty();
      }
      else
      {
        s.unregister_port(port);
        port.address = n;
        s.register_port(port);
        g.mark_dirty();
      }
    };
  }
  else if (auto ad = address.address.toString_unsafe().toStdString();
           ossia::traversal::is_pattern(ad))
  {
    // OPTIMIZEME
    auto path = ossia::traversal::make_path(ad);
    if (path)
    {
      return [&s, &g, path=std::move(path)] (T& port) {
        s.unregister_port(port);
        port.address = std::move(*path);
        if (ossia::value_port* dat
            = port.template target<ossia::value_port>())
        {
          dat->type = {};
          dat->index.clear();
        }
        s.register_port(port);
        g.mark_dirty();
      };
    }
    else
    {
      return [&s, &g] (T& port) {
        s.unregister_port(port);
        port.address = {};
        if (ossia::value_port* dat
            = port.template target<ossia::value_port>())
        {
          dat->type = {};
          dat->index.clear();
        }
        g.mark_dirty();
      };
    }
  }
  else
  {
    return [] (T& port) { };
  }
}

class control_surface_node : public ossia::nonowning_graph_node
{
public:
  std::vector<std::pair<ossia::value*, bool>> controls;

  std::pair<ossia::value*, bool>& add_control()
  {
    auto inletport = new ossia::value_inlet;
    auto outletport = new ossia::value_outlet;
    controls.push_back({new ossia::value, false});
    m_inlets.push_back(inletport);
    m_outlets.push_back(outletport);
    return controls.back();
  }

  struct control_updater
  {
    std::pair<ossia::value*, bool>& control;
    ossia::value v;

    void operator()() noexcept
    {
      *control.first = std::move(v);
      control.second = true;
    }
  };

  void run(const token_request&, exec_state_facade) noexcept override
  {
    // TODO take input port data into account.
    const int n = std::ssize(controls);
    for (int i = 0; i < n; i++)
    {
      auto& ctl = controls[i];
      if (ctl.second)
      {
        m_outlets[i]->target<ossia::value_port>()->write_value(
            std::move(*ctl.first), 0);
        ctl.second = false;
      }
    }
  }

  std::string label() const noexcept override { return "control surface"; }
};
}

namespace ControlSurface
{

struct con_unvalidated
{
  const Execution::Context& ctx;
  const int i;
  std::weak_ptr<ossia::control_surface_node> weak_node;
  void operator()(const ossia::value& val)
  {
    if (auto node = weak_node.lock())
    {
      ctx.executionQueue.enqueue(ossia::control_surface_node::control_updater{
          node->controls[i], val});
    }
  }
};

ProcessExecutorComponent::ProcessExecutorComponent(Model& element, const Execution::Context& ctx, QObject* parent)
  : Execution::ProcessComponent_T<
    ControlSurface::Model,
    ossia::node_process>{element, ctx, "ControlSurface", parent}
{
  auto node = ossia::make_node<ossia::control_surface_node>(*ctx.execState);
  this->node = node;
  this->m_ossia_process = std::make_shared<ossia::node_process>(this->node);

  // Initialize all the controls in the node with the current value.
  // And update the node when the UI changes

  // TODO do it also when they change
  const auto& map = element.outputAddresses();
  for (auto& ctl : element.inlets())
  {
    std::pair<ossia::value*, bool>& p = node->add_control();
    auto ctrl = safe_cast<Process::ControlInlet*>(ctl);
    *p.first = ctrl->value(); // TODO does this make sense ?
    p.second = true;          // we will send the first value

    const State::AddressAccessor& addr = map.at(ctl->id().val());
    system().setup.set_destination(addr, node->root_outputs().back());

    std::weak_ptr<ossia::control_surface_node> weak_node = node;
    QObject::connect(
          ctrl,
          &Process::ControlInlet::valueChanged,
          this,
          con_unvalidated{ctx, m_currentIndex++, weak_node});
  }

  QObject::connect(&element, &Model::controlAdded,
                  this, [this] (const Id<Process::Port>& id) {
    auto& inl = *static_cast<Process::ControlInlet*>(this->process().inlet(id));
    inletAdded(inl);
  });
}

void ProcessExecutorComponent::inletAdded(Process::ControlInlet& inl)
{
  auto& ctx = this->system();
  const auto& map = this->process().outputAddresses();

  auto v = inl.value(); // TODO does this make sense ?

  const State::AddressAccessor& addr = map.at(inl.id().val());
  auto set_addr = set_destination_impl<ossia::outlet>(*ctx.execState, *ctx.execGraph, addr);

  std::weak_ptr<ossia::control_surface_node> weak_node = std::dynamic_pointer_cast<ossia::control_surface_node>(this->node);
  in_exec([v, weak_node, set_addr] () mutable {
    if(auto node = weak_node.lock()) {
      std::pair<ossia::value*, bool>& p = node->add_control();
      *p.first = std::move(v);
      p.second = true;

      set_addr(*node->root_outputs().back());
    }
  });

  // Note: we cannot remove inlets in the exec as this would shift all the indices.
  // We cannot use pointers / refs in con_unvalidated as they may be invalidated upon reallocation.
  QObject::connect(
        &inl,
        &Process::ControlInlet::valueChanged,
        this,
        con_unvalidated{ctx, m_currentIndex++, weak_node});
}

ProcessExecutorComponent::~ProcessExecutorComponent() { }

}
