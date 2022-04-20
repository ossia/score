#pragma once

#include <Crousti/ProcessModel.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/GpuNode.hpp>
#include <Crousti/GpuComputeNode.hpp>
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
#include <Media/AudioDecoder.hpp>
namespace oscr
{
namespace
{
// TODO refactor this into a generic explicit soundfile loaded mechanism
static auto loadSoundfile(Process::ControlInlet* inlet, double rate)
{
  // Initialize the control with the current soundfile
  if(auto str = inlet->value().target<std::string>())
  {
    auto dec = Media::AudioDecoder::decode_synchronous(
                 QString::fromStdString(*str),
                 rate);

    if(dec.has_value())
    {
      auto hdl = std::make_shared<ossia::audio_data>();
      hdl->data = std::move(dec->second);
      hdl->path = std::move(*str);
      return hdl;
    }
  }
  return ossia::audio_handle{};
}

static auto loadSoundfile(Process::ControlInlet* inlet, const std::shared_ptr<ossia::execution_state>& st)
{
  const double rate = ossia::exec_state_facade{st.get()}.sampleRate();
  return loadSoundfile(inlet, rate);
}
}

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

  template <typename Field, std::size_t N, std::size_t NField>
  constexpr void operator()(Field& param, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[NField]);

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


  template <avnd::soundfile_port Field, std::size_t N, std::size_t NField>
  void operator()(Field& param, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[NField]);

    // First we can load it directly since execution hasn't started yet
    auto& sf = param.soundfile;
    if(auto hdl = loadSoundfile(inlet, ctx.execState))
      node_ptr->soundfile_loaded(hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});

    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = node_ptr;
    std::weak_ptr<ossia::execution_state> weak_st = ctx.execState;
    QObject::connect(
        inlet,
        &Process::ControlInlet::valueChanged,
        parent,
          [inlet, &ctx=this->ctx, weak_node = std::move(weak_node), weak_st = std::move(weak_st)] {
           auto n = weak_node.lock();
           if(!n)
             return;
           auto st = weak_st.lock();
           if(!st)
             return;

           if(auto file = loadSoundfile(inlet, st))
           {
             ctx.executionQueue.enqueue([f=std::move(file), weak_node] () mutable {
               auto n = weak_node.lock();
               if(!n)
                 return;

               // We store the sound file handle returned in this lambda so that it gets
               // GC'd in the main thread
               n->soundfile_loaded(f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
             });
           }
        });
  }
};

template <typename Node>
struct ApplyEngineControlChangeToUI
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  typename ExecNode::control_input_values_type& arr;
  Model& element;

  template <std::size_t N, std::size_t NField>
  void operator()(auto& field, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[NField]);
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

  template <std::size_t N, std::size_t NField>
  void operator()(auto& field, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    auto outlet = static_cast<Process::ControlOutlet*>(element.outlets()[NField]);
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
      avnd::control_input_introspection<Node>::for_all_n2(
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
      avnd::control_output_introspection<Node>::for_all_n2(
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
  type_if<int, is_gpu<Node>> node_id = -1;

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
    if constexpr(is_gpu<Node>)
    {
      auto& gfx_exec = ctx.doc.plugin<Gfx::DocumentPlugin>().exec;

      // Create the executor in the audio thread
      auto node = std::make_shared<Gfx::gfx_exec_node>(gfx_exec);
      node->prepare(*ctx.execState);

      this->node = node;

      // Create the controls, inputs outputs etc.
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
        if (auto ctrl = qobject_cast<Process::ControlOutlet*>(outlet))
        {
          node->add_control_out();
        }
        else if (auto ctrl = qobject_cast<Process::ValueOutlet*>(outlet))
        {
          node->add_control_out();
        }
        else if (auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
        {
          node->add_texture_out();
          out->nodeId = node_id;
        }
      }

      // Create the GPU node
      if constexpr(GpuGraphicsNode2<Node>)
      {
        node->id = gfx_exec.ui->register_node(std::make_unique<CustomGpuNode<Node>>());
      }
      else if constexpr(GpuComputeNode2<Node>)
      {
        auto& q = ctx.executionQueue;
        node->id = gfx_exec.ui->register_node(std::make_unique<GpuComputeNode<Node>>(q, node->control_outs));
      }
      else if constexpr(GpuNode<Node>)
      {
        auto state = std::make_shared<Node>();
        node->id = gfx_exec.ui->register_node(std::make_unique<GfxNode<Node>>(state));
      }
      node_id = node->id;
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
      using soundfile_inputs_type = avnd::soundfile_input_introspection<Node>;
      using control_outputs_type = avnd::control_output_introspection<Node>;
      avnd::effect_container<Node>& eff = node->impl;

      // UI controls to engine
      if constexpr (control_inputs_type::size > 0)
      {
        // Initialize all the controls in the node with the current value.
        // And update the node when the UI changes
        control_inputs_type::for_all_n2(
              avnd::get_inputs<Node>(eff),
              setup_Impl0<Node>{element, ctx, ptr, this}
        );
      }
      if constexpr (soundfile_inputs_type::size > 0)
      {
        soundfile_inputs_type::for_all_n2(
              avnd::get_inputs<Node>(eff),
              setup_Impl0<Node>{element, ctx, ptr, this}
              );
      }

      // Custom UI messages to engine
      if constexpr(requires { element.from_ui; }) {
        element.from_ui = [p = QPointer{this}, &eff] (QByteArray b) {
          if(!p)
            return;

          p->in_exec([mess = std::move(b), &eff] {
            using refl = avnd::function_reflection<&Node::process_message>;
            static_assert(refl::count <= 1);

            if constexpr(refl::count == 0) {
              // no arguments, just call it
              eff.effect.process_message();
            }
            else if constexpr(refl::count == 1) {
              using arg_type = avnd::first_argument<&Node::process_message>;
              std::decay_t<arg_type> arg;
              MessageBusReader b{mess};
              b(arg);
              eff.effect.process_message(std::move(arg));
            }
          });
        };
      }

      if constexpr(requires { eff.effect.send_message; }) {
        eff.effect.send_message = [this] (auto b) mutable {
          this->in_edit([this, bb = std::move(b)] () mutable{
            MessageBusSender{this->process().to_ui}(std::move(bb));
          });
        };
      }

      // Engine to ui controls
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
    if constexpr(requires { this->process().from_ui; }) {
      this->process().from_ui = [] (QByteArray arr) { };
    }
    // FIXME cleanup eff.effect.send_message too ?

#if SCORE_PLUGIN_GFX
    if constexpr(is_gpu<Node>)
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


