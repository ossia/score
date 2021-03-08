#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/safe_nodes/executor.hpp>

#include <ControlSurface/Process.hpp>

namespace ossia
{
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
    const int n = controls.size();
    for (int i = 0; i < n; i++)
    {
      auto& ctl = controls[i];
      if (ctl.second)
      {
        m_outlets[i]->target<ossia::value_port>()->write_value(std::move(*ctl.first), 0);
        ctl.second = false;
      }
    }
  }

  std::string label() const noexcept override { return "control surface"; }
};

}
namespace ControlSurface
{
class Model;

struct con_unvalidated
{
  const Execution::Context& ctx;
  const int i;
  std::weak_ptr<ossia::control_surface_node> weak_node;
  void operator()(const ossia::value& val)
  {
    if (auto node = weak_node.lock())
    {
      ctx.executionQueue.enqueue(
          ossia::control_surface_node::control_updater{node->controls[i], val});
    }
  }
};

class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<ControlSurface::Model, ossia::node_process>
{
  COMPONENT_METADATA("bab572b1-37eb-4f32-8f72-d5b79b65cfe9")
public:
  ProcessExecutorComponent(
      ControlSurface::Model& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
      : Execution::ProcessComponent_T<ControlSurface::Model, ossia::node_process>{
          element,
          ctx,
          id,
          "ControlSurface",
          parent}
  {
    std::shared_ptr<ossia::control_surface_node> node
        = std::make_shared<ossia::control_surface_node>();
    this->node = node;
    this->m_ossia_process = std::make_shared<ossia::node_process>(this->node);

    // Initialize all the controls in the node with the current value.
    // And update the node when the UI changes

    // TODO do it also when they change
    const auto& map = element.outputAddresses();
    int i = 0;
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
          ctrl, &Process::ControlInlet::valueChanged, this, con_unvalidated{ctx, i, weak_node});
      i++;
    }
  }

  ~ProcessExecutorComponent();
};
using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
