#pragma once

#include <Crousti/ProcessModel.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/GpuNode.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <Engine/Node/TickPolicy.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/tools/Bind.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>

#if SCORE_PLUGIN_GFX
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Crousti/GpuNode.hpp>
#endif

#include <QTimer>
#include <avnd/binding/ossia/node.hpp>
#include <avnd/binding/ossia/mono_audio_node.hpp>
#include <avnd/binding/ossia/poly_audio_node.hpp>


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

template <typename Node>
struct setup_Impl0
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  Model& element;
  const Execution::Context& ctx;
  const std::shared_ptr<ExecNode>& node_ptr;
  QObject* parent;

  template <typename Field, std::size_t N>
  struct con_unvalidated
  {
    const Execution::Context& ctx;
    std::weak_ptr<ExecNode> weak_node;
    Field& field;
    void operator()(const ossia::value& val)
    {
      constexpr auto control_index = N;

      using control_value_type = std::decay_t<decltype(Field::value)>;

      if (auto node = weak_node.lock())
      {
        control_value_type v;
        oscr::from_ossia_value(field, val, v);
        ctx.executionQueue.enqueue(
              control_updater<ExecNode, control_value_type, control_index>{
                weak_node,
                std::move(v)
              }
        );
      }
    }
  };

  template <typename Field, std::size_t N>
  constexpr void operator()(Field& param, avnd::num<N>)
  {
    constexpr int port_index = avnd::control_input_introspection<Node>::index_map[N];
    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[port_index]);

    // Initialize the control with the current value of the inlet
    oscr::from_ossia_value(param, inlet->value(), param.value);

    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = node_ptr;
    QObject::connect(
        inlet,
        &Process::ControlInlet::valueChanged,
        parent,
        con_unvalidated<Field, N>{ctx, weak_node, param});
  }
};

template <typename Node>
struct ApplyEngineControlChangeToUI
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  typename ExecNode::control_input_values_type& arr;
  Model& element;

  template <std::size_t N>
  void operator()(auto& field, avnd::num<N>)
  {
    constexpr int port_index = avnd::control_input_introspection<Node>::index_map[N];
    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[port_index]);
    inlet->setExecutionValue(oscr::to_ossia_value(field.value));
  }
};

template <typename Node>
struct setup_Impl1_Out
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;
  typename ExecNode::control_output_values_type& arr;
  Model& element;

  template <std::size_t N>
  void operator()(auto& field, avnd::num<N>)
  {
    constexpr int port_index = avnd::control_output_introspection<Node>::index_map[N];
    auto outlet = static_cast<Process::ControlOutlet*>(element.outlets()[port_index]);
    outlet->setValue(oscr::to_ossia_value(field.value));
  }
};

template <typename Node>
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
    while (node.control.ins_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if (ok)
    {
      avnd::control_input_introspection<Node>::for_all_n(
            avnd::get_inputs<Node>(node.impl),
            ApplyEngineControlChangeToUI<Node>{arr, element}
      );
    }
  }

  void handle_control_outs(ExecNode& node) const noexcept
  {
    using namespace ossia::safe_nodes;
    // TODO disconnect the connection ? it will be disconnected shortly
    // after...
    typename ExecNode::control_output_values_type arr;
    bool ok = false;
    while (node.control.outs_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if (ok)
    {
      avnd::control_output_introspection<Node>::for_all_n(
            avnd::get_outputs<Node>(node.impl),
            setup_Impl1_Out<Node>{arr, element}
      );
    }
  }

  void operator()() const noexcept
  {
    if (auto node = weak_node.lock())
    {
      constexpr const auto control_count = avnd::control_input_introspection<Node>::size;
      constexpr const auto control_out_count = avnd::control_output_introspection<Node>::size;
      if constexpr (control_count > 0)
        handle_controls(*node);

      if constexpr (control_out_count > 0)
        handle_control_outs(*node);
    }
  }
};

template<typename T, bool Predicate>
struct type_if;
template<typename T>
struct type_if<T, false>
{
  type_if() = default;
  type_if(const type_if&) = default;
  type_if(type_if&&) = default;
  type_if& operator=(const type_if&) = default;
  type_if& operator=(type_if&&) = default;

  template<typename U>
  type_if(U&&) { }
  template<typename U>
  T& operator=(U&& u) noexcept { return *this; }
};

template<typename T>
struct type_if<T, true>
{
  [[no_unique_address]] T value;

  type_if() = default;
  type_if(const type_if&) = default;
  type_if(type_if&&) = default;
  type_if& operator=(const type_if&) = default;
  type_if& operator=(type_if&&) = default;

  template<typename U>
  type_if(U&& other): value{std::forward<U>(other)} { }

  operator const T&() const noexcept { return value; }
  operator T&() noexcept { return value; }
  operator T&&() && noexcept { return std::move(value); }

  template<typename U>
  T& operator=(U&& u) noexcept { return value = std::forward<U>(u); }
};

template <typename Node>
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

  [[no_unique_address]]
  type_if<int, GpuNode<Node>> node_id = -1;

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
      node->prepare();

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
        else if (auto ctrl = qobject_cast<Process::ValueInlet*>(ctl))
        {
          auto& p = node->add_control();
          p->changed = true;
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

      // FIXME refactor this with other GFX processes
      for(auto* outlet : element.outlets())
      {
        if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
        {
          out->nodeId = node_id;
        }
      }

    }
    else
#endif
    {
      auto st = ossia::exec_state_facade{ctx.execState.get()};
      std::shared_ptr<safe_node<Node>> ptr;
      auto node = new safe_node<Node>{st.bufferSize(), (double)st.sampleRate()};
      node->prepare(*ctx.execState.get());
      ptr.reset(node);
      this->node = ptr;

      using control_inputs_type = avnd::control_input_introspection<Node>;
      using control_outputs_type = avnd::control_output_introspection<Node>;
      auto& eff = node->impl;
      if constexpr (control_inputs_type::size > 0)
      {
        // Initialize all the controls in the node with the current value.
        // And update the node when the UI changes
        avnd::control_input_introspection<Node>::for_all_n(
              avnd::get_inputs<Node>(eff),
              setup_Impl0<Node>{element, ctx, ptr, this}
        );
      }

      if constexpr (control_inputs_type::size > 0 || control_outputs_type::size > 0)
      {
        // Update the value in the UI
        std::weak_ptr<safe_node<Node>> weak_node = ptr;
        ExecutorGuiUpdate<Node> timer_action{weak_node, element};
        timer_action();

        con(ctx.doc.coarseUpdateTimer,
            &QTimer::timeout,
            this,
            [=] { timer_action(); },
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
      // FIXME this must move in the Node dtor. See video_node
      auto& gfx_exec = this->system().doc.template plugin<Gfx::DocumentPlugin>().exec;
      if (node_id >= 0)
        gfx_exec.ui->unregister_node(node_id);
    }

    // FIXME refactor this with other GFX processes
    for(auto* outlet : this->process().outlets())
    {
      if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
      {
        out->nodeId = -1;
      }
    }
#endif
    ::Execution::ProcessComponent::cleanup();
  }

  ~Executor() {

  }
};
}


