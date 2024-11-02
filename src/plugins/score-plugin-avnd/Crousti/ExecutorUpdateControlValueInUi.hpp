#pragma once
#include <Process/Process.hpp>

#include <Crousti/ProcessModel.hpp>

#include <avnd/binding/ossia/node.hpp>

namespace oscr
{
template <typename Node>
struct update_control_in_value_in_ui
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  typename ExecNode::control_input_values_type& arr;
  Model& element;

  template <avnd::dynamic_ports_port Field, std::size_t N, std::size_t NField>
  void operator()(Field& field, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    for(auto p : element.avnd_input_idx_to_model_ports(NField))
    {
      if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
      {
        using namespace std;
        // FIXME handle dynamic ports correctly
        // inlet->setExecutionValue(oscr::to_ossia_value(field, get<N>(arr)));
      }
    }
  }

  template <typename Field, std::size_t N, std::size_t NField>
  void operator()(Field& field, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    auto p = element.avnd_input_idx_to_model_ports(NField)[0];
    if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
    {
      using namespace std;
      inlet->setExecutionValue(oscr::to_ossia_value(field, get<N>(arr)));
    }
  }
};

template <typename Node>
struct update_control_out_value_in_ui
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  typename ExecNode::control_output_values_type& arr;
  Model& element;

  template <typename Field, std::size_t N, std::size_t NField>
  void operator()(Field& field, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    using namespace std;
    auto ports = element.avnd_output_idx_to_model_ports(NField);
    SCORE_ASSERT(!ports.empty());
    auto outlet = safe_cast<Process::ControlOutlet*>(ports[0]);
    outlet->setValue(oscr::to_ossia_value(field, get<N>(arr)));
  }

  template <avnd::dynamic_ports_port Field, std::size_t N, std::size_t NField>
  void operator()(Field& field, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    using namespace std;
    // FIXME handle dynamic ports correctly
    // auto outlet
    //     = safe_cast<Process::ControlOutlet*>(modelPort<Node>(element.outlets(), NField));
    // outlet->setValue(oscr::to_ossia_value(field, get<N>(arr)));
  }
};

template <typename Node>
struct update_control_value_in_ui
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  std::weak_ptr<ExecNode> weak_node;
  Model& element;

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
      for(auto state : node.impl.full_state())
      {
        avnd::control_input_introspection<Node>::for_all_n2(
            state.inputs, update_control_in_value_in_ui<Node>{arr, element});
      }
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
      // FIXME not thread safe?
      avnd::control_output_introspection<Node>::for_all_n2(
          avnd::get_outputs<Node>(node.impl),
          update_control_out_value_in_ui<Node>{arr, element});
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
