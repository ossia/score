#pragma once

#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/CpuGeneratorNode.hpp>
#include <Crousti/GpuComputeNode.hpp>
#include <Crousti/GpuNode.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/context.hpp>

#include <ossia-qt/invoke.hpp>

#include <QGuiApplication>

#if SCORE_PLUGIN_GFX
#include <Crousti/GpuNode.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#endif

#include <Media/AudioDecoder.hpp>

#include <score/tools/ThreadPool.hpp>

#include <ossia/detail/type_if.hpp>

#include <QTimer>

#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/mono_audio_node.hpp>
#include <avnd/binding/ossia/node.hpp>
#include <avnd/binding/ossia/ossia_audio_node.hpp>
#include <avnd/binding/ossia/poly_audio_node.hpp>
#include <avnd/concepts/temporality.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/concepts/worker.hpp>
#include <libremidi/reader.hpp>

namespace oscr
{
namespace
{
[[nodiscard]] static QString
filenameFromPort(const ossia::value& value, const score::DocumentContext& ctx)
{
  if(auto str = value.target<std::string>())
    return score::locateFilePath(QString::fromStdString(*str).trimmed(), ctx);
  return {};
}

// TODO refactor this into a generic explicit soundfile loaded mechanism
[[nodiscard]] static auto
loadSoundfile(const ossia::value& value, const score::DocumentContext& ctx, double rate)
{
  // Initialize the control with the current soundfile
  if(auto str = filenameFromPort(value, ctx); !str.isEmpty())
  {
    auto dec = Media::AudioDecoder::decode_synchronous(str, rate);

    if(dec.has_value())
    {
      auto hdl = std::make_shared<ossia::audio_data>();
      hdl->data = std::move(dec->second);
      hdl->path = str.toStdString();
      hdl->rate = rate;
      return hdl;
    }
  }
  return ossia::audio_handle{};
}

using midifile_handle = std::shared_ptr<oscr::midifile_data>;
[[nodiscard]] inline midifile_handle
loadMidifile(const ossia::value& value, const score::DocumentContext& ctx)
{
  // Initialize the control with the current soundfile
  if(auto str = filenameFromPort(value, ctx); !str.isEmpty())
  {
    QFile f(str);
    if(!f.open(QIODevice::ReadOnly))
      return {};
    auto ptr = f.map(0, f.size());

    auto hdl = std::make_shared<oscr::midifile_data>();
    if(auto ret = hdl->reader.parse((uint8_t*)ptr, f.size());
       ret == libremidi::reader::invalid)
      return {};

    hdl->filename = str.toStdString();
    return hdl;
  }
  return {};
}

using raw_file_handle = std::shared_ptr<raw_file_data>;
[[nodiscard]] inline raw_file_handle loadRawfile(
    const ossia::value& value, const score::DocumentContext& ctx, bool text, bool mmap)
{
  // Initialize the control with the current soundfile
  if(auto filename = filenameFromPort(value, ctx); !filename.isEmpty())
  {
    if(!QFile::exists(filename))
      return {};

    auto hdl = std::make_shared<oscr::raw_file_data>();
    hdl->file.setFileName(filename);
    if(!hdl->file.open(QIODevice::ReadOnly))
      return {};

    if(mmap)
    {
      auto map = (char*)hdl->file.map(0, hdl->file.size());
      hdl->data = QByteArray::fromRawData(map, hdl->file.size());
    }
    else
    {
      if(text)
        hdl->file.setTextModeEnabled(true);

      hdl->data = hdl->file.readAll();
    }
    hdl->filename = filename.toStdString();
    return hdl;
  }
  return {};
}
[[nodiscard]] inline auto loadSoundfile(
    const ossia::value& value, const score::DocumentContext& ctx,
    const std::shared_ptr<ossia::execution_state>& st)
{
  const double rate = ossia::exec_state_facade{st.get()}.sampleRate();
  return loadSoundfile(value, ctx, rate);
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

template <typename ExecNode_T, typename T, std::size_t ControlN>
struct control_updater_dynamic_port
{
  std::weak_ptr<ExecNode_T> weak_node;
  int port_index;
  T v;
  void operator()()
  {
    if(auto n = weak_node.lock())
    {
      n->template control_updated_from_ui<T, ControlN>(std::move(v), port_index);
    }
  }
};

template <typename Node, typename Field, std::size_t NPred, std::size_t NField>
struct con_unvalidated
{
  using ExecNode = safe_node<Node>;
  const Execution::Context& ctx;
  std::weak_ptr<ExecNode> weak_node;
  Field& field;
  void operator()(const ossia::value& val)
  {
    constexpr auto control_index = NPred;

    using control_value_type = std::decay_t<decltype(Field::value)>;

    if(auto node = weak_node.lock())
    {
      control_value_type v;
      node->from_ossia_value(field, val, v, avnd::field_index<NField>{});
      ctx.executionQueue.enqueue(
          control_updater<ExecNode, control_value_type, control_index>{
              weak_node, std::move(v)});
    }
  }
};

template <typename Node, typename Field, std::size_t NPred, std::size_t NField>
struct con_unvalidated_dynamic_port
{
  using ExecNode = safe_node<Node>;
  const Execution::Context& ctx;
  std::weak_ptr<ExecNode> weak_node;
  Field& field;
  int port_index;
  void operator()(const ossia::value& val)
  {
    constexpr auto control_index = NPred;

    using control_value_type = std::decay_t<decltype(Field::value)>;

    if(auto node = weak_node.lock())
    {
      control_value_type v;
      node->from_ossia_value(field, val, v, avnd::field_index<NField>{});
      ctx.executionQueue.enqueue(
          control_updater_dynamic_port<ExecNode, control_value_type, control_index>{
              weak_node, port_index, std::move(v)});
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

  template <typename Field, std::size_t N, std::size_t NField>
  constexpr void
  operator()(Field& param, avnd::predicate_index<N> np, avnd::field_index<NField> nf)
  {
    const auto ports = element.avnd_input_idx_to_model_ports(NField);

    if constexpr(avnd::dynamic_ports_port<Field>)
    {
      param.ports.resize(ports.size());
    }

    int k = 0;
    for(auto p : ports)
    {
      if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
      {
        // Initialize the control with the current value of the inlet if it is not an optional
        if constexpr(avnd::dynamic_ports_port<Field>)
        {
          if constexpr(!requires { param.ports[0].value.reset(); })
          {
            node_ptr->from_ossia_value(param, inlet->value(), param.ports[k].value, nf);
          }
        }
        else
        {
          if constexpr(!requires { param.value.reset(); })
          {
            node_ptr->from_ossia_value(param, inlet->value(), param.value, nf);
          }
        }

        {
          avnd::effect_container<Node>& eff = node_ptr->impl;
          {
            for(auto state : eff.full_state())
            {
              // FIXME dynamic_ports
              if_possible(param.update(state.effect));
            }
          }
        }

        // Connect to changes
        std::weak_ptr<ExecNode> weak_node = node_ptr;
        if constexpr(avnd::dynamic_ports_port<Field>)
        {
          using port_type = avnd::dynamic_port_type<Field>;
          QObject::connect(
              inlet, &Process::ControlInlet::valueChanged, parent,
              con_unvalidated_dynamic_port<Node, port_type, N, NField>{
                  ctx, weak_node, param.ports[k], k});
        }
        else
        {
          QObject::connect(
              inlet, &Process::ControlInlet::valueChanged, parent,
              con_unvalidated<Node, Field, N, NField>{ctx, weak_node, param});
        }
      }
      k++;
    }
    // Else it's an unhandled value inlet
  }

  template <avnd::soundfile_port Field, std::size_t N, std::size_t NField>
  void operator()(Field& param, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    for(auto p : element.avnd_input_idx_to_model_ports(NField))
    {
      if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
      {
        // FIXME handle dynamic ports correctly
        // First we can load it directly since execution hasn't started yet
        if(auto hdl = loadSoundfile(inlet->value(), ctx.doc, ctx.execState))
          node_ptr->soundfile_loaded(
              hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});

        // Connect to changes
        std::weak_ptr<ExecNode> weak_node = node_ptr;
        std::weak_ptr<ossia::execution_state> weak_st = ctx.execState;
        QObject::connect(
            inlet, &Process::ControlInlet::valueChanged, parent,
            [&ctx = this->ctx, weak_node = std::move(weak_node),
             weak_st = std::move(weak_st)](const ossia::value& v) {
          if(auto n = weak_node.lock())
            if(auto st = weak_st.lock())
              if(auto file = loadSoundfile(v, ctx.doc, st))
              {
                ctx.executionQueue.enqueue([f = std::move(file), weak_node]() mutable {
                  auto n = weak_node.lock();
                  if(!n)
                    return;

                  // We store the sound file handle returned in this lambda so that it gets
                  // GC'd in the main thread
                  n->soundfile_loaded(
                      f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
                });
              }
        });
      }
    }
  }

  template <avnd::midifile_port Field, std::size_t N, std::size_t NField>
  void operator()(Field& param, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    for(auto p : element.avnd_input_idx_to_model_ports(NField))
    {
      if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
      {
        // FIXME handle dynamic ports correctly

        // First we can load it directly since execution hasn't started yet
        if(auto hdl = loadMidifile(inlet->value(), ctx.doc))
          node_ptr->midifile_loaded(
              hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});

        // Connect to changes
        std::weak_ptr<ExecNode> weak_node = node_ptr;
        std::weak_ptr<ossia::execution_state> weak_st = ctx.execState;
        QObject::connect(
            inlet, &Process::ControlInlet::valueChanged, parent,
            [inlet, &ctx = this->ctx,
             weak_node = std::move(weak_node)](const ossia::value& v) {
          if(auto n = weak_node.lock())
            if(auto file = loadMidifile(v, ctx.doc))
            {
              ctx.executionQueue.enqueue([f = std::move(file), weak_node]() mutable {
                auto n = weak_node.lock();
                if(!n)
                  return;

                // We store the sound file handle returned in this lambda so that it gets
                // GC'd in the main thread
                n->midifile_loaded(
                    f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
              });
            }
        });
      }
    }
  }

  template <typename Field>
  static auto executePortPreprocess(auto& file)
  {
    using field_file_type = decltype(Field::file);
    field_file_type ffile;
    ffile.bytes = decltype(ffile.bytes)(file.data.constData(), file.file.size());
    ffile.filename = file.filename;
    return Field::process(ffile);
  }

  template <avnd::raw_file_port Field, std::size_t N, std::size_t NField>
  void operator()(Field& param, avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    for(auto p : element.avnd_input_idx_to_model_ports(NField))
    {
      if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
      {
        // FIXME handle dynamic ports correctly

        using file_ports = avnd::raw_file_input_introspection<Node>;
        using elt = typename file_ports::template nth_element<N>;
        constexpr bool has_text = requires { decltype(elt::file)::text; };
        constexpr bool has_mmap = requires { decltype(elt::file)::mmap; };

        // First we can load it directly since execution hasn't started yet
        if(auto hdl = loadRawfile(inlet->value(), ctx.doc, has_text, has_mmap))
        {
          if constexpr(avnd::port_can_process<Field>)
          {
            // FIXME also do it when we get a run-time message from the exec engine,
            // OSC, etc
            auto func = executePortPreprocess<Field>(*hdl);
            node_ptr->file_loaded(
                hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
            if(func)
              func(node_ptr->impl.effect);
          }
          else
          {
            node_ptr->file_loaded(
                hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
          }
        }

        // Connect to changes
        std::weak_ptr<ExecNode> weak_node = node_ptr;
        std::weak_ptr<ossia::execution_state> weak_st = ctx.execState;
        QObject::connect(
            inlet, &Process::ControlInlet::valueChanged, parent,
            [inlet, &ctx = this->ctx, weak_node = std::move(weak_node)] {
          if(auto n = weak_node.lock())
            if(auto file = loadRawfile(inlet->value(), ctx.doc, has_text, has_mmap))
            {
              if constexpr(avnd::port_can_process<Field>)
              {
                auto func = executePortPreprocess<Field>(*file);
                ctx.executionQueue.enqueue(
                    [f = std::move(file), weak_node, ff = std::move(func)]() mutable {
                  auto n = weak_node.lock();
                  if(!n)
                    return;

                  // We store the sound file handle returned in this lambda so that it gets
                  // GC'd in the main thread
                  n->file_loaded(
                      f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
                  if(ff)
                    ff(n->impl.effect);
                });
              }
              else
              {
                ctx.executionQueue.enqueue([f = std::move(file), weak_node]() mutable {
                  auto n = weak_node.lock();
                  if(!n)
                    return;

                  // We store the sound file handle returned in this lambda so that it gets
                  // GC'd in the main thread
                  n->file_loaded(
                      f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
                });
              }
            }
        });
      }
    }
  }
};

template <typename Node>
struct ApplyEngineControlChangeToUI
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
struct setup_Impl1_Out
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
    while(node.control.ins_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if(ok)
    {
      for(auto state : node.impl.full_state())
      {
        avnd::control_input_introspection<Node>::for_all_n2(
            state.inputs, ApplyEngineControlChangeToUI<Node>{arr, element});
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
      avnd::control_output_introspection<Node>::for_all_n2(
          avnd::get_outputs<Node>(node.impl), setup_Impl1_Out<Node>{arr, element});
    }
  }

  void operator()() const noexcept
  {
    if(auto node = weak_node.lock())
    {
      constexpr const auto control_count = avnd::control_input_introspection<Node>::size;
      constexpr const auto control_out_count
          = avnd::control_output_introspection<Node>::size;
      if constexpr(control_count > 0)
        handle_controls(*node);

      if constexpr(control_out_count > 0)
        handle_control_outs(*node);
    }
  }
};

template <typename Node>
class CustomNodeProcess : public ossia::node_process
{
  using node_process::node_process;
  void start() override
  {
    node_process::start();
    auto& n = static_cast<safe_node<Node>&>(*node);
    n.impl.effect.start();
  }
  void stop() override
  {
    auto& n = static_cast<safe_node<Node>&>(*node);
    n.impl.effect.stop();
    node_process::stop();
  }
};

template <typename Node>
class Executor final
    : public Execution::ProcessComponent_T<ProcessModel<Node>, ossia::node_process>
{
public:
  static Q_DECL_RELAXED_CONSTEXPR UuidKey<score::Component> static_key() noexcept
  {
    return uuid_from_string<Node>();
  }

  UuidKey<score::Component> key() const noexcept final override { return static_key(); }

  bool key_match(UuidKey<score::Component> other) const noexcept final override
  {
    return static_key() == other || Execution::ProcessComponent::base_key_match(other);
  }

  [[no_unique_address]] ossia::type_if<int, is_gpu<Node>> node_id = -1;

  Executor(ProcessModel<Node>& element, const ::Execution::Context& ctx, QObject* p)
      : Execution::ProcessComponent_T<ProcessModel<Node>, ossia::node_process>{
          element, ctx, "Executor::ProcessModel<Info>", p}
  {
    const auto id
        = std::hash<ObjectPath>{}(Path<Process::ProcessModel>{element}.unsafePath());
#if SCORE_PLUGIN_GFX
    if constexpr(is_gpu<Node>)
    {
      auto& gfx_exec = ctx.doc.plugin<Gfx::DocumentPlugin>().exec;

      // Create the executor in the audio thread
      struct named_exec_node final : Gfx::gfx_exec_node
      {
        using Gfx::gfx_exec_node::gfx_exec_node;
        std::string label() const noexcept override
        {
          return std::string(avnd::get_name<Node>());
        }
      };

      auto node = std::make_shared<named_exec_node>(gfx_exec);
      node->prepare(*ctx.execState);

      this->node = node;

      // Create the controls, inputs outputs etc.
      std::size_t i = 0;
      for(auto& ctl : element.inlets())
      {
        if(auto ctrl = qobject_cast<Process::ControlInlet*>(ctl))
        {
          auto& p = node->add_control();
          p->value = ctrl->value();
          p->changed = true;

          QObject::connect(
              ctrl, &Process::ControlInlet::valueChanged, this,
              Gfx::con_unvalidated{ctx, i, 0, node});
          i++;
        }
        else if(auto ctrl = qobject_cast<Process::ValueInlet*>(ctl))
        {
          auto& p = node->add_control();
          p->changed = true;
          i++;
        }
        else if(auto ctrl = qobject_cast<Process::AudioInlet*>(ctl))
        {
          node->add_audio();
        }
        else if(auto ctrl = qobject_cast<Gfx::TextureInlet*>(ctl))
        {
          node->add_texture();
        }
      }

      // FIXME refactor this with other GFX processes
      for(auto* outlet : element.outlets())
      {
        if(auto ctrl = qobject_cast<Process::ControlOutlet*>(outlet))
        {
          node->add_control_out();
        }
        else if(auto ctrl = qobject_cast<Process::ValueOutlet*>(outlet))
        {
          node->add_control_out();
        }
        else if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
        {
          node->add_texture_out();
          out->nodeId = node_id;
        }
      }

      // Create the GPU node

      std::weak_ptr qex_ptr = std::shared_ptr<Execution::ExecutionCommandQueue>(
          ctx.alias.lock(), &ctx.executionQueue);
      std::unique_ptr<score::gfx::Node> ptr;
      if constexpr(GpuGraphicsNode2<Node>)
      {
        auto gpu_node = new CustomGpuNode<Node>(qex_ptr, node->control_outs, id);
        ptr.reset(gpu_node);
      }
      else if constexpr(GpuComputeNode2<Node>)
      {
        auto gpu_node = new GpuComputeNode<Node>(qex_ptr, node->control_outs, id);
        ptr.reset(gpu_node);
      }
      else if constexpr(GpuNode<Node>)
      {
        auto gpu_node = new GfxNode<Node>(element, qex_ptr, node->control_outs, id);
        ptr.reset(gpu_node);
      }
      node->id = gfx_exec.ui->register_node(std::move(ptr));
      node_id = node->id;
    }
    else
#endif
    {
      auto st = ossia::exec_state_facade{ctx.execState.get()};
      std::shared_ptr<safe_node<Node>> ptr;
      auto node = new safe_node<Node>{st.bufferSize(), (double)st.sampleRate(), id};
      node->root_inputs().reserve(element.inlets().size());
      node->root_outputs().reserve(element.outlets().size());

      node->prepare(*ctx.execState.get()); // Preparation of the ossia side

      auto& net_ctx
          = *ctx.doc.findPlugin<Explorer::DeviceDocumentPlugin>()->networkContext();
      if_possible(node->impl.effect.ossia_state = st);
      if_possible(node->impl.effect.io_context = &net_ctx.context);
      ptr.reset(node);
      this->node = ptr;

      if constexpr(requires { ptr->impl.effect; })
        if constexpr(std::is_same_v<std::decay_t<decltype(ptr->impl.effect)>, Node>)
          connect_message_bus(element, ctx, ptr->impl.effect);
      connect_worker(ctx, ptr->impl);

      node->dynamic_ports = element.dynamic_ports;
      node->finish_init();

      connect_controls(element, ctx, ptr);
      update_controls(ptr);
      QObject::connect(
          &element, &Process::ProcessModel::inletsChanged, this,
          &Executor::recompute_ports);
      QObject::connect(
          &element, &Process::ProcessModel::outletsChanged, this,
          &Executor::recompute_ports);

      // To call prepare() after evertyhing is ready
      node->audio_configuration_changed(st);

      m_oldInlets = element.inlets();
      m_oldOutlets = element.outlets();
    }

    if constexpr(avnd::tag_process_exec<Node>)
    {
      this->m_ossia_process = std::make_shared<CustomNodeProcess<Node>>(this->node);
    }
    else
    {
      this->m_ossia_process = std::make_shared<ossia::node_process>(this->node);
    }
  }

  Process::Inlets m_oldInlets;
  Process::Outlets m_oldOutlets;
  void recompute_ports()
  {
    Execution::Transaction commands{this->system()};
    auto n = std::dynamic_pointer_cast<safe_node<Node>>(this->node);
    if(!n)
      return;

    // Re-run setup_inlets ?
    this->in_exec([dp = this->process().dynamic_ports, node = n] {
      node->dynamic_ports = dp;
      node->root_inputs().clear();
      node->root_outputs().clear();
      node->initialize_all_ports();
    });
  }

  void connect_controls(
      ProcessModel<Node>& element, const ::Execution::Context& ctx,
      std::shared_ptr<safe_node<Node>>& ptr)
  {
    using dynamic_ports_port_type = avnd::dynamic_ports_input_introspection<Node>;
    using control_inputs_type = avnd::control_input_introspection<Node>;
    using curve_inputs_type = avnd::curve_input_introspection<Node>;
    using soundfile_inputs_type = avnd::soundfile_input_introspection<Node>;
    using midifile_inputs_type = avnd::midifile_input_introspection<Node>;
    using raw_file_inputs_type = avnd::raw_file_input_introspection<Node>;
    using control_outputs_type = avnd::control_output_introspection<Node>;

    // UI controls to engine
    safe_node<Node>& node = *ptr;
    avnd::effect_container<Node>& eff = node.impl;

    if constexpr(dynamic_ports_port_type::size > 0)
    {
      // Initialize all the controls in the node with the current value.
      // And update the node when the UI changes

      for(auto state : eff.full_state())
      {
        dynamic_ports_port_type::for_all_n2(
            state.inputs, setup_Impl0<Node>{element, ctx, ptr, this});
      }
    }
    if constexpr(control_inputs_type::size > 0)
    {
      // Initialize all the controls in the node with the current value.
      // And update the node when the UI changes

      for(auto state : eff.full_state())
      {
        control_inputs_type::for_all_n2(
            state.inputs, setup_Impl0<Node>{element, ctx, ptr, this});
      }
    }
    if constexpr(curve_inputs_type::size > 0)
    {
      // Initialize all the controls in the node with the current value.
      // And update the node when the UI changes

      for(auto state : eff.full_state())
      {
        curve_inputs_type::for_all_n2(
            state.inputs, setup_Impl0<Node>{element, ctx, ptr, this});
      }
    }
    if constexpr(soundfile_inputs_type::size > 0)
    {
      soundfile_inputs_type::for_all_n2(
          avnd::get_inputs<Node>(eff), setup_Impl0<Node>{element, ctx, ptr, this});

      auto& tq = score::TaskPool::instance();
      node.soundfiles.load_request
          = [&tq, p = std::weak_ptr{ptr}, &ctx](std::string& str, int idx) {
        auto eff_ptr = p.lock();
        if(!eff_ptr)
          return;
        tq.post([eff_ptr = std::move(eff_ptr), filename = str, &ctx, idx]() mutable {
          if(auto file = loadSoundfile(filename, ctx.doc, ctx.execState))
          {
            ctx.executionQueue.enqueue(
                [sf = std::move(file), p = std::weak_ptr{eff_ptr}, idx]() mutable {
              auto eff_ptr = p.lock();
              if(!eff_ptr)
                return;

              avnd::effect_container<Node>& eff = eff_ptr->impl;
              soundfile_inputs_type::for_nth_mapped_n2(
                  avnd::get_inputs<Node>(eff), idx,
                  [&]<std::size_t NField, std::size_t N>(
                      auto& field, avnd::predicate_index<N> p,
                      avnd::field_index<NField> f) {
                eff_ptr->soundfile_loaded(sf, p, f);
              });
            });
          }
        });
      };
    }
    if constexpr(midifile_inputs_type::size > 0)
    {
      midifile_inputs_type::for_all_n2(
          avnd::get_inputs<Node>(eff), setup_Impl0<Node>{element, ctx, ptr, this});
    }
    if constexpr(raw_file_inputs_type::size > 0)
    {
      raw_file_inputs_type::for_all_n2(
          avnd::get_inputs<Node>(eff), setup_Impl0<Node>{element, ctx, ptr, this});
    }

    // Engine to ui controls
    if constexpr(control_inputs_type::size > 0 || control_outputs_type::size > 0)
    {
      // Update the value in the UI
      std::weak_ptr<safe_node<Node>> weak_node = ptr;
      ExecutorGuiUpdate<Node> timer_action{weak_node, element};
      timer_action();

      con(
          ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [=] { timer_action(); },
          Qt::QueuedConnection);
    }
  }

  void connect_message_bus(
      ProcessModel<Node>& element, const ::Execution::Context& ctx, Node& eff)
  {
    // Custom UI messages to engine
    if constexpr(avnd::has_gui_to_processor_bus<Node>)
    {
      element.from_ui = [p = QPointer{this}, &eff](QByteArray b) {
        if(!p)
          return;

        p->in_exec([mess = std::move(b), &eff]() mutable {
          using refl = avnd::function_reflection<&Node::process_message>;
          static_assert(refl::count <= 1);

          if constexpr(refl::count == 0)
          {
            // no arguments, just call it
            eff.process_message();
          }
          else if constexpr(refl::count == 1)
          {
            using arg_type = avnd::first_argument<&Node::process_message>;
            std::decay_t<arg_type> arg;
            MessageBusReader reader{mess};
            reader(arg);
            eff.process_message(std::move(arg));
          }
        });
      };
    }

    if constexpr(avnd::has_processor_to_gui_bus<Node>)
    {
      eff.send_message = [self = QPointer{this}]<typename T>(T&& b) mutable {
        if(!self)
          return;
        if constexpr(
            sizeof(QPointer<QObject>) + sizeof(b)
            < Execution::ExecutionCommand::max_storage)
        {
          self->in_edit(
              [proc = QPointer{&self->process()}, bb = std::move(b)]() mutable {
            if(proc->to_ui)
              MessageBusSender{proc->to_ui}(std::move(bb));
          });
        }
        else
        {
          self->in_edit(
              [proc = QPointer{&self->process()},
               bb = std::make_unique<std::decay_t<T>>(std::move(b))]() mutable {
            if(proc->to_ui)
              MessageBusSender{proc->to_ui}(*std::move(bb));
          });
        }
      };
    }
  }

  void connect_worker(const ::Execution::Context& ctx, avnd::effect_container<Node>& eff)
  {
    if constexpr(avnd::has_worker<Node>)
    {
      // Initialize the thread pool beforehand
      auto& tq = score::TaskPool::instance();
      using worker_type = decltype(eff.effect.worker);
      for(auto& eff : eff.effects())
      {
        std::weak_ptr eff_ptr = std::shared_ptr<Node>(this->node, &eff);
        std::weak_ptr qex_ptr = std::shared_ptr<Execution::ExecutionCommandQueue>(
            ctx.alias.lock(), &ctx.executionQueue);

        eff.worker.request
            = [&tq, qex_ptr = std::move(qex_ptr),
               eff_ptr = std::move(eff_ptr)]<typename... Args>(Args&&... f) mutable {
          // request() is invoked in the DSP / processor thread
          // and just posts the task to the thread pool
          tq.post([eff_ptr = std::move(eff_ptr), qex_ptr = std::move(qex_ptr),
                   ... ff = std::forward<Args>(f)]() mutable {
            // This happens in the worker thread
            // If for some reason the object has already been removed, not much
            // reason to perform the work
            if(!eff_ptr.lock())
              return;

            using type_of_result
                = decltype(worker_type::work(std::forward<decltype(ff)>(ff)...));
            if constexpr(std::is_void_v<type_of_result>)
            {
              worker_type::work(std::forward<decltype(ff)>(ff)...);
            }
            else
            {
              // If the worker returns a std::function, it
              // is to be invoked back in the processor DSP thread
              auto res = worker_type::work(std::forward<decltype(ff)>(ff)...);
              if(!res)
                return;

              // Execution queue is currently spsc from main thread to an exec thread,
              // we cannot just yeet the result back from the thread-pool
              ossia::qt::run_async(
                  qApp, [eff_ptr = std::move(eff_ptr), qex_ptr = std::move(qex_ptr),
                         res = std::move(res)]() mutable {
                    // Main thread
                    std::shared_ptr qex = qex_ptr.lock();
                    if(!qex)
                      return;

                    qex->enqueue(
                        [eff_ptr = std::move(eff_ptr), res = std::move(res)]() mutable {
                  // DSP / processor thread
                  // We need res to be mutable so that the worker can use it to e.g. store
                  // old data which will be freed back in the main thread
                  if(auto p = eff_ptr.lock())
                    res(*p);
                    });
                  });
            }
          });
        };
      }
    }
  }

  // Update everything
  void update_controls(std::shared_ptr<safe_node<Node>>& ptr)
  {
    avnd::effect_container<Node>& eff = ptr->impl;
    {
      for(auto state : eff.full_state())
      {
        avnd::input_introspection<Node>::for_all(
            state.inputs, [&](auto& field) { if_possible(field.update(state.effect)); });
      }
    }
  }

  void cleanup() override
  {
    if constexpr(requires { this->process().from_ui; })
    {
      this->process().from_ui = [](QByteArray arr) {};
    }
    // FIXME cleanup eff.effect.send_message too ?

#if SCORE_PLUGIN_GFX
    if constexpr(is_gpu<Node>)
    {
      // FIXME this must move in the Node dtor. See video_node
      auto& gfx_exec = this->system().doc.template plugin<Gfx::DocumentPlugin>().exec;
      if(node_id >= 0)
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

  ~Executor() { }
};
}
