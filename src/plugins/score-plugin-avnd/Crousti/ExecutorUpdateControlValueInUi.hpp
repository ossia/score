#pragma once
#include <Process/Process.hpp>

#include <Crousti/ProcessModel.hpp>

#include <avnd/binding/ossia/node.hpp>
#include <utility>

namespace oscr
{
template <typename Node>
struct update_control_in_value_in_ui
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  typename ExecNode::control_input_values_type& arr;
  QPointer<Model> element;

  template <std::size_t N>
  void operator()()
  {
    using Info = avnd::control_input_introspection<Node>;
    using Field = typename Info::template nth_element<N>;
    if constexpr(!avnd::dynamic_ports_port<Field>)
    {
      if(!element)
        return;
      auto p = element->avnd_input_idx_to_model_ports(Info::index_to_field_index(N))[0];
      if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
      {
        using std::get;
        inlet->setExecutionValue(oscr::to_ossia_value(get<N>(arr)));
      }
    }
  }
};

template <typename Node>
struct update_control_out_value_in_ui
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  typename ExecNode::control_output_values_type& arr;
  QPointer<Model> element;

  template <std::size_t N>
  void operator()()
  {
    using Info = avnd::control_output_introspection<Node>;
    using Field = typename Info::template nth_element<N>;
    if constexpr(!avnd::dynamic_ports_port<Field>)
    {
      if(!element)
        return;
      using std::get;
      auto ports = element->avnd_output_idx_to_model_ports(Info::index_to_field_index(N));
      SCORE_ASSERT(!ports.empty());
      auto outlet = safe_cast<Process::ControlOutlet*>(ports[0]);
      outlet->setValue(oscr::to_ossia_value(get<N>(arr)));
    }
  }
};

template <typename Node>
struct update_control_value_in_ui
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  std::weak_ptr<ExecNode> weak_node;
  QPointer<Model> element;

  void handle_controls(ExecNode& node) const noexcept
  {
    using namespace ossia::safe_nodes;
    // TODO disconnect the connection ? it will be disconnected shortly
    // after...

    typename ExecNode::control_input_values_type arr;
    bool ok = false;
    while(node.control.ins_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if(ok)
    {
      // Values already came through the queue. Do not inspect live instances:
      // audio channel negotiation can resize their storage on another thread.
      auto publish = update_control_in_value_in_ui<Node>{arr, element};
      [&]<std::size_t... N>(std::index_sequence<N...>) {
        (publish.template operator()<N>(), ...);
      }(std::make_index_sequence<avnd::control_input_introspection<Node>::size>{});
    }
  }

  void handle_control_outs(ExecNode& node) const noexcept
  {
    using namespace ossia::safe_nodes;
    // TODO disconnect the connection ? it will be disconnected shortly
    // after...
    typename ExecNode::control_output_values_type arr;
    bool ok = false;
    while(node.control.outs_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if(ok)
    {
      // The queued tuple contains the first channel's output controls.
      auto publish = update_control_out_value_in_ui<Node>{arr, element};
      [&]<std::size_t... N>(std::index_sequence<N...>) {
        (publish.template operator()<N>(), ...);
      }(std::make_index_sequence<avnd::control_output_introspection<Node>::size>{});
    }
  }

  void operator()() const noexcept
  {
    if(auto node = weak_node.lock())
    {
      static constexpr const auto control_count
          = avnd::control_input_introspection<Node>::size;
      static constexpr const auto control_out_count
          = avnd::control_output_introspection<Node>::size;
      if constexpr(control_count > 0)
        handle_controls(*node);

      if constexpr(control_out_count > 0)
        handle_control_outs(*node);
    }
  }
};
}
