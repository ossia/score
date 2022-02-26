#pragma once
#include <oscr/ProcessModel.hpp>
#include <oscr/ExecutorNode.hpp>
#include <oscr/GpuNode.hpp>

#include <Engine/Node/TickPolicy.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#if SCORE_PLUGIN_GFX
#include <Gfx/GfxApplicationPlugin.hpp>
#endif

#include <QTimer>


namespace oscr
{

template <typename ExecNode_T, typename T, std::size_t ControlN>
struct control_updater
{
  std::weak_ptr<ExecNode_T> weak_node;
  T v;
  void operator()()
  {
    if(auto n = weak_node.lock())
    {
      n->template control_updated_from_ui<T, ControlN>(std::move(v));
    }
  }
};

template <DataflowNode Node>
struct setup_Impl0
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  Model& element;
  const Execution::Context& ctx;
  const std::shared_ptr<ExecNode>& node_ptr;
  QObject* parent;

  template <typename ControlIndexT>
  struct con_validated
  {
    const Execution::Context& ctx;
    std::weak_ptr<ExecNode> weak_node;
    void operator()(const ossia::value& val)
    {
      using namespace boost::pfr;
      using namespace ossia::safe_nodes;
      using Info = Node;
      constexpr auto control_index = ControlIndexT::value;
      //using port_index_t = typename inlet_reflection<Info>::template control_input_index<ControlIndexT::value>;
      using control_tuple = typename inlet_reflection<Info>::control_input_tuple;
      using control_member = std::tuple_element_t<ControlIndexT::value, control_tuple>;
      constexpr auto control_spec = get_control<control_member>();
      using control_type = decltype(control_spec);

      using control_value_type = typename control_type::type;

      if (auto node = weak_node.lock())
      {
        if (auto v = control_spec.fromValue(val))
          ctx.executionQueue.enqueue(control_updater<ExecNode, control_value_type, control_index>{weak_node, std::move(*v)});
      }
    }
  };

  template <typename ControlIndexT>
  struct con_unvalidated
  {
    const Execution::Context& ctx;
    std::weak_ptr<ExecNode> weak_node;
    void operator()(const ossia::value& val)
    {
      using namespace boost::pfr;
      using namespace ossia::safe_nodes;
      using Info = Node;
      constexpr auto control_index = ControlIndexT::value;
      //using port_index_t = typename inlet_reflection<Info>::template control_input_index<ControlIndexT::value>;

      using control_tuple = typename inlet_reflection<Info>::control_input_tuple;
      using control_member = std::tuple_element_t<ControlIndexT::value, control_tuple>;
      constexpr auto control_spec = get_control<control_member>();
      using control_type = decltype(control_spec);

      using control_value_type = typename control_type::type;

      if (auto node = weak_node.lock())
      {
        ctx.executionQueue.enqueue(control_updater<ExecNode, control_value_type, control_index>{weak_node, control_spec.fromValue(val)});
      }
    }
  };

  template <typename ControlIndexT>
  constexpr void operator()(ControlIndexT)
  {
    using namespace boost::pfr;
    using namespace ossia::safe_nodes;
    using Info = Node;
    constexpr int control_index = ControlIndexT::value;
    using port_index_t = typename inlet_reflection<Info>::template control_input_index<ControlIndexT::value>;
    using control_tuple = typename inlet_reflection<Info>::control_input_tuple;
    using control_member = std::tuple_element_t<ControlIndexT::value, control_tuple>;
    constexpr auto control_spec = get_control<control_member>();
    using control_type = decltype(control_spec);

    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[port_index_t::value]);

    auto& node = *node_ptr;
    std::weak_ptr<ExecNode> weak_node = node_ptr;

    if constexpr (control_type::must_validate)
    {
      if (auto res = control_spec.fromValue(inlet->value()))
        get<control_index>(node.control_input) = *res;

      QObject::connect(
          inlet,
          &Process::ControlInlet::valueChanged,
          parent,
          con_validated<ControlIndexT>{ctx, weak_node});
    }
    else
    {
      get<control_index>(node.control_input) = control_spec.fromValue(inlet->value());

      QObject::connect(
          inlet,
          &Process::ControlInlet::valueChanged,
          parent,
          con_unvalidated<ControlIndexT>{ctx, weak_node});
    }
  }
};

template <DataflowNode Node>
struct ApplyEngineControlChangeToUI
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  typename ExecNode::control_input_values_type& arr;
  Model& element;

  template <typename ControlIndexT>
  void operator()(ControlIndexT)
  {
    using namespace boost::pfr;
    using namespace ossia::safe_nodes;

    constexpr int control_index = ControlIndexT::value;
    using port_index_t = typename inlet_reflection<Node>::template control_input_index<ControlIndexT::value>;

    using control_tuple = typename inlet_reflection<Node>::control_input_tuple;
    using control_member = std::tuple_element_t<ControlIndexT::value, control_tuple>;
    constexpr auto control_spec = get_control<control_member>();
    //using control_type = decltype(control_spec);

    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[port_index_t::value]);

    inlet->setExecutionValue(control_spec.toValue(get<control_index>(arr)));
  }
};

template <DataflowNode Node>
struct setup_Impl1_Out
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  typename ExecNode::control_output_values_type& arr;
  Model& element;

  template <typename ControlIndexT>
  void operator()(ControlIndexT)
  {
    using namespace boost::pfr;
    using namespace ossia::safe_nodes;
    constexpr int control_index = ControlIndexT::value;
    using port_index_t = typename outlet_reflection<Node>::template control_output_index<ControlIndexT::value>;

    constexpr const auto control_spec = tuple_element_t<port_index_t::value, decltype(ExecNode::state.outputs)>::display();

    auto outlet = static_cast<Process::ControlOutlet*>(element.outlets()[port_index_t::value]);

    outlet->setValue(control_spec.toValue(get<control_index>(arr)));
  }
};

template <DataflowNode Node>
struct ExecutorGuiUpdate
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
    while (node.control_ins_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if (ok)
    {
      constexpr const auto control_count = inlet_reflection<Node>::control_in_count;

      ossia::for_each_in_range<control_count>(ApplyEngineControlChangeToUI<Node>{arr, element});
    }
  }

  void handle_control_outs(ExecNode& node) const noexcept
  {
    using namespace ossia::safe_nodes;
    // TODO disconnect the connection ? it will be disconnected shortly
    // after...
    typename ExecNode::control_output_values_type arr;
    bool ok = false;
    while (node.control_outs_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if (ok)
    {
      constexpr const auto control_out_count
          = outlet_reflection<Node>::control_out_count;

      ossia::for_each_in_range<control_out_count>(
          setup_Impl1_Out<Node>{arr, element});
    }
  }

  void operator()() const noexcept
  {
    using namespace ossia::safe_nodes;
    if (auto node = weak_node.lock())
    {
      if constexpr (inlet_reflection<Node>::control_in_count > 0)
        handle_controls(*node);

      if constexpr (outlet_reflection<Node>::control_out_count > 0)
        handle_control_outs(*node);
    }
  }
};

template <DataflowNode Node>
class Executor final
    : public Execution::
          ProcessComponent_T<ProcessModel<Node>, ossia::node_process>
{
public:
  static Q_DECL_RELAXED_CONSTEXPR UuidKey<score::Component>
  static_key() noexcept
  {
    return uuid_from_string<Node>();
  }

  UuidKey<score::Component> key() const noexcept final override
  {
    return static_key();
  }

  bool key_match(UuidKey<score::Component> other) const noexcept final override
  {
    return static_key() == other
           || Execution::ProcessComponent::base_key_match(other);
  }

  int node_id = -1;
  Executor(
      ProcessModel<Node>& element,
      const ::Execution::Context& ctx,
      QObject* p)
      : Execution::
          ProcessComponent_T<ProcessModel<Node>, ossia::node_process>{
              element,
              ctx,
              "Executor::ProcessModel<Info>",
              p}
  {
#if SCORE_PLUGIN_GFX
    if constexpr(GpuNode<Node>)
    {
      auto& gfx_exec = ctx.doc.plugin<Gfx::DocumentPlugin>().exec;
      // Create the matching gpu node
      auto state = std::make_shared<Node>();
      auto node = std::make_shared<Gfx::gfx_exec_node>(gfx_exec);

      node->root_outputs().push_back(new ossia::texture_outlet);
      this->node = node;
      node->id = gfx_exec.ui->register_node(std::make_unique<GfxNode<Node>>(state));
      node_id = node->id;

      std::size_t i = 0;
      for (auto& ctl : element.inlets())
      {
        if (auto ctrl = qobject_cast<Process::ControlInlet*>(ctl))
        {
          auto& p = node->add_control();
          p->value = ctrl->value();
          p->changed = true;

          QObject::connect(
              ctrl,
              &Process::ControlInlet::valueChanged,
              this,
              Gfx::con_unvalidated{ctx, i, 0, node});
          i++;
        }
        else if (auto ctrl = qobject_cast<Process::AudioInlet*>(ctl))
        {
          node->add_audio();
        }
        else if (auto ctrl = qobject_cast<Gfx::TextureInlet*>(ctl))
        {
          node->add_texture();
        }
      }
    }
    else
#endif
    {
      auto node = std::make_shared<safe_node<Node>>(ossia::exec_state_facade{ctx.execState.get()});
      this->node = node;

      if constexpr (HasControlInputs<Node>)
      {
        // Initialize all the controls in the node with the current value.
        // And update the node when the UI changes
        ossia::for_each_in_range<inlet_reflection<Node>::control_in_count>(
            setup_Impl0<Node>{element, ctx, node, this});
      }

      if constexpr (HasControlInputs<Node> || HasControlOutputs<Node>)
      {
        // Update the value in the UI
        std::weak_ptr<safe_node<Node>> weak_node = node;
        con(ctx.doc.coarseUpdateTimer,
            &QTimer::timeout,
            this,
            ExecutorGuiUpdate<Node>{weak_node, element},
            Qt::QueuedConnection);
      }
    }

    this->m_ossia_process = std::make_shared<ossia::node_process>(this->node);
  }

  void cleanup() override
  {
#if SCORE_PLUGIN_GFX
    if constexpr(GpuNode<Node>)
    {
      auto& gfx_exec = this->system().doc.template plugin<Gfx::DocumentPlugin>().exec;
      if (node_id >= 0)
        gfx_exec.ui->unregister_node(node_id);
    }
#endif
    Executor::ProcessComponent::cleanup();
  }

  ~Executor() {

  }
};
}
