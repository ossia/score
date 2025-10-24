#pragma once

#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/ExecutorPortSetup.hpp>
#include <Crousti/ExecutorUpdateControlValueInUi.hpp>
#include <Crousti/File.hpp>
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

#include <flicks.h>

#if SCORE_PLUGIN_GFX
#include <Crousti/GpuNode.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#endif

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

namespace oscr
{

template <typename Node>
class CustomNodeProcess : public ossia::node_process
{
  using node_process::node_process;
  void start() override
  {
    node_process::start();
    auto& n = static_cast<safe_node<Node>&>(*node);
    if_possible(n.impl.effect.start());
  }
  void pause() override
  {
    auto& n = static_cast<safe_node<Node>&>(*node);
    if_possible(n.impl.effect.pause());
    node_process::pause();
  }

  void resume() override
  {
    node_process::resume();
    auto& n = static_cast<safe_node<Node>&>(*node);
    if_possible(n.impl.effect.resume());
  }

  void stop() override
  {
    auto& n = static_cast<safe_node<Node>&>(*node);
    if_possible(n.impl.effect.stop());
    node_process::stop();
  }

  void offset_impl(ossia::time_value date) override
  {
    node_process::offset_impl(date);
    auto& n = static_cast<safe_node<Node>&>(*node);
    util::flicks f{date.impl};
    if_possible(n.impl.effect.transport(f));
  }

  void transport_impl(ossia::time_value date) override
  {
    node_process::transport_impl(date);
    auto& n = static_cast<safe_node<Node>&>(*node);

    util::flicks f{date.impl};
    if_possible(n.impl.effect.transport(f));
  }
};

template <typename Node>
class Executor final
    : public Execution::ProcessComponent_T<ProcessModel<Node>, ossia::node_process>
{
  Process::Inlets m_oldInlets;
  Process::Outlets m_oldOutlets;

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
    if constexpr(is_gpu<Node>)
    {
#if SCORE_PLUGIN_GFX
      setup_gpu(element, ctx, p);
#endif
    }
    else
    {
      setup_cpu(element, ctx, p);
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

  void
  setup_cpu(ProcessModel<Node>& element, const ::Execution::Context& ctx, QObject* p)
  {
    auto& net_ctx
        = *ctx.doc.findPlugin<Explorer::DeviceDocumentPlugin>()->networkContext();
    const auto id
        = std::hash<ObjectPath>{}(Path<Process::ProcessModel>{element}.unsafePath());

    auto st = ossia::exec_state_facade{ctx.execState.get()};
    std::shared_ptr<safe_node<Node>> ptr;
    auto node = new safe_node<Node>{st.bufferSize(), (double)st.sampleRate(), id};
    node->root_inputs().reserve(element.inlets().size());
    node->root_outputs().reserve(element.outlets().size());

    node->prepare(*ctx.execState.get()); // Preparation of the ossia side

    if_possible(node->impl.effect.ossia_state = st);
    if_possible(node->impl.effect.io_context = &net_ctx.context);
    if_possible(node->impl.effect.ossia_document_context = &ctx.doc);
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

  void
  setup_gpu(ProcessModel<Node>& element, const ::Execution::Context& ctx, QObject* p)
  {
#if SCORE_PLUGIN_GFX
    // FIXME net context for gpu node ?
    const int64_t id
        = std::hash<ObjectPath>{}(Path<Process::ProcessModel>{element}.unsafePath());

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
        ossia::texture_inlet& inl = *node->add_texture();
        ctrl->setupExecution(inl, this);
      }
      else if(auto ctrl = qobject_cast<Gfx::GeometryInlet*>(ctl))
      {
        ossia::geometry_inlet& inl = *node->add_geometry();
        ctrl->setupExecution(inl, this);
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
      else if(auto out = qobject_cast<Gfx::GeometryOutlet*>(outlet))
      {
        node->add_geometry_out();
      }
    }

    // Create the GPU node
    std::weak_ptr qex_ptr = std::shared_ptr<Execution::ExecutionCommandQueue>(
        ctx.alias.lock(), &ctx.executionQueue);
    std::unique_ptr<score::gfx::Node> ptr;
    if constexpr(GpuGraphicsNode2<Node>)
    {
      auto gpu_node = new CustomGpuNode<Node>(qex_ptr, node->control_outs, id, ctx.doc);
      ptr.reset(gpu_node);
    }
    else if constexpr(GpuComputeNode2<Node>)
    {
      auto gpu_node = new GpuComputeNode<Node>(qex_ptr, node->control_outs, id, ctx.doc);
      ptr.reset(gpu_node);
    }
    else if constexpr(GpuNode<Node>)
    {
      auto gpu_node
          = new GfxNode<Node>(element, qex_ptr, node->control_outs, id, ctx.doc);
      ptr.reset(gpu_node);
    }

    i = 0;
    for(auto& ctl : element.inlets())
    {
      if(auto ctrl = qobject_cast<Gfx::TextureInlet*>(ctl))
      {
        ossia::texture_inlet& inl
            = static_cast<ossia::texture_inlet&>(*node->root_inputs()[i]);
        ptr->process(i, inl.data); // Setup render_target_spec
      }
      i++;
    }
    node->id = gfx_exec.ui->register_node(std::move(ptr));
    node_id = node->id;
#endif
  }

  void recompute_ports()
  {
    Execution::Transaction commands{this->system()};
    auto n = std::dynamic_pointer_cast<safe_node<Node>>(this->node);
    if(!n)
      return;

    // Re-run setup_inlets ?
    in_exec([dp = this->process().dynamic_ports, node = n] {
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

    // Initialize all the controls in the node with the current value.
    // And update the node when the UI changes

    if constexpr(dynamic_ports_port_type::size > 0)
    {
      for(auto state : eff.full_state())
      {
        dynamic_ports_port_type::for_all_n2(
            state.inputs, dispatch_control_setup<Node>{element, ctx, ptr, this});
      }
    }
    if constexpr(control_inputs_type::size > 0)
    {
      for(auto state : eff.full_state())
      {
        control_inputs_type::for_all_n2(
            state.inputs, dispatch_control_setup<Node>{element, ctx, ptr, this});
      }
    }
    if constexpr(curve_inputs_type::size > 0)
    {
      for(auto state : eff.full_state())
      {
        curve_inputs_type::for_all_n2(
            state.inputs, dispatch_control_setup<Node>{element, ctx, ptr, this});
      }
    }
    if constexpr(soundfile_inputs_type::size > 0)
    {
      soundfile_inputs_type::for_all_n2(
          avnd::get_inputs<Node>(eff),
          dispatch_control_setup<Node>{element, ctx, ptr, this});

      setup_soundfile_task_pool(element, ctx, ptr);
    }
    if constexpr(midifile_inputs_type::size > 0)
    {
      midifile_inputs_type::for_all_n2(
          avnd::get_inputs<Node>(eff),
          dispatch_control_setup<Node>{element, ctx, ptr, this});
    }
    if constexpr(raw_file_inputs_type::size > 0)
    {
      raw_file_inputs_type::for_all_n2(
          avnd::get_inputs<Node>(eff),
          dispatch_control_setup<Node>{element, ctx, ptr, this});
    }

    // Engine to ui controls
    if constexpr(control_inputs_type::size > 0 || control_outputs_type::size > 0)
    {
      // Update the value in the UI
      std::weak_ptr<safe_node<Node>> weak_node = ptr;
      update_control_value_in_ui<Node> timer_action{weak_node, &element};
      timer_action();

      con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this,
          [timer_action = std::move(timer_action)] { timer_action(); },
          Qt::QueuedConnection);
    }
  }

  void setup_soundfile_task_pool(
      ProcessModel<Node>& element, const ::Execution::Context& ctx,
      std::shared_ptr<safe_node<Node>>& ptr)
  {
    safe_node<Node>& node = *ptr;

    using soundfile_inputs_type = avnd::soundfile_input_introspection<Node>;

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

  void connect_message_bus(
      ProcessModel<Node>& element, const ::Execution::Context& ctx, Node& eff)
  {
    // Custom UI messages to engine
    if constexpr(avnd::has_gui_to_processor_bus<Node>)
    {
      element.from_ui = [qex_ptr = weak_exec, &eff](QByteArray b) {
        auto qex = qex_ptr.lock();
        if(!qex)
          return;

        qex->enqueue([mess = std::move(b), &eff]() mutable {
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
      if constexpr(requires { eff.send_message = [](auto&&) { }; })
      {
        eff.send_message = [proc = QPointer{&this->process()},
                            qed_ptr = weak_edit]<typename T>(T&& b) mutable {
          auto qed = qed_ptr.lock();
          if(!qed)
            return;
          if constexpr(
              sizeof(QPointer<QObject>) + sizeof(b)
              < Execution::ExecutionCommand::max_storage)
          {
            qed->enqueue([proc, bb = std::move(b)]() mutable {
              if(proc && proc->to_ui)
                MessageBusSender{proc->to_ui}(std::move(bb));
            });
          }
          else
          {
            qed->enqueue(
                [proc, bb = std::make_unique<std::decay_t<T>>(std::move(b))]() mutable {
              if(proc && proc->to_ui)
                MessageBusSender{proc->to_ui}(*std::move(bb));
            });
          }
        };
      }
      else if constexpr(requires { eff.send_message = []() { }; })
      {
        eff.send_message
            = [proc = QPointer{&this->process()}, qed_ptr = weak_edit]() mutable {
          if(!proc)
            return;
          auto qed = qed_ptr.lock();
          if(!qed)
            return;

          qed->enqueue([proc]() mutable {
            if(proc && proc->to_ui)
              MessageBusSender{proc->to_ui}();
          });
        };
      }
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
          tq.post([eff_ptr, qex_ptr, ... ff = std::forward<Args>(f)]() mutable {
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
