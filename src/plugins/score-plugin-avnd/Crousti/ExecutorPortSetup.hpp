#pragma once
#include <Process/Process.hpp>

#include <Crousti/File.hpp>
#include <Crousti/ProcessModel.hpp>

#include <avnd/binding/ossia/node.hpp>

namespace oscr
{

template <typename Node, typename Field, std::size_t NPred, std::size_t NField>
struct con_unvalidated
{
  using ExecNode = safe_node<Node>;
  const Execution::Context& ctx;
  std::weak_ptr<ExecNode> weak_node;
  Field& field;
  void operator()(const ossia::value& val)
  {
    using control_value_type = std::decay_t<decltype(Field::value)>;

    if(auto node = weak_node.lock())
    {
      control_value_type v;
      node->from_ossia_value(field, val, v, avnd::field_index<NField>{});
      ctx.executionQueue.enqueue([weak_node = weak_node, v = std::move(v)]() mutable {
        if(auto n = weak_node.lock())
        {
          n->template control_updated_from_ui<control_value_type, NPred>(std::move(v));
        }
      });
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
    using control_value_type = std::decay_t<decltype(Field::value)>;

    if(auto node = weak_node.lock())
    {
      control_value_type v;
      node->from_ossia_value(field, val, v, avnd::field_index<NField>{});
      ctx.executionQueue.enqueue(
          [weak_node = weak_node, port_index = port_index, v = std::move(v)]() mutable {
        if(auto n = weak_node.lock())
        {
          n->template control_updated_from_ui<control_value_type, NPred>(
              std::move(v), port_index);
        }
      });
    }
  }
};

template <typename Node, typename Field>
struct setup_control_for_exec_base
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  Model& element;
  const Execution::Context& ctx;
  const std::shared_ptr<ExecNode>& node_ptr;
  QObject* parent;

  void invoke_update(Field& param)
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
};
template <typename Node, typename Field, std::size_t N, std::size_t NField>
struct setup_control_for_exec;

template <typename Node, typename Field, std::size_t N, std::size_t NField>
struct setup_control_for_exec : setup_control_for_exec_base<Node, Field>
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  void initialize_control(Field& param, Process::ControlInlet* inlet, int k)
  {
    // Initialize the control with the current value of the inlet if it is not an optional
    if constexpr(avnd::dynamic_ports_port<Field>)
    {
      if constexpr(!requires { param.ports[0].value.reset(); })
      {
        this->node_ptr->from_ossia_value(
            param, inlet->value(), param.ports[k].value, avnd::field_index<NField>{});
      }
    }
    else
    {
      if constexpr(!requires { param.value.reset(); })
      {
        this->node_ptr->from_ossia_value(
            param, inlet->value(), param.value, avnd::field_index<NField>{});
      }
    }
  }

  void update_controller(Field& param, Process::ControlInlet* inlet)
  {
    // FIXME proper tag
    if constexpr(requires { param.update_controller; })
    {
      param.update_controller
          = [inlet = QPointer{inlet}, self = QPointer{&this->element}](auto&& value) {
        if(!self || !inlet)
          return;

        // Notify the UI if the object has the power
        // to actually change the value of the control
        // FIXME better to use in_edit queue ?
        // FIXME not too efficient but which choice do we have ?
        static thread_local const Field field;
        ossia::qt::run_async(
            qApp, [self, inlet, val = oscr::to_ossia_value(field, value)] {
          if(!self || !inlet)
            return;
          inlet->setValue(val);
        });
      };
    }
  }

  void connect_control_to_ui(Field& param, Process::ControlInlet* inlet, int k)
  {
    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    if constexpr(avnd::dynamic_ports_port<Field>)
    {
      using port_type = avnd::dynamic_port_type<Field>;
      QObject::connect(
          inlet, &Process::ControlInlet::valueChanged, this->parent,
          con_unvalidated_dynamic_port<Node, port_type, N, NField>{
              this->ctx, weak_node, param.ports[k], k});

      this->update_controller(param, inlet);
    }
    else
    {
      QObject::connect(
          inlet, &Process::ControlInlet::valueChanged, this->parent,
          con_unvalidated<Node, Field, N, NField>{this->ctx, weak_node, param});

      this->update_controller(param, inlet);
    }
  }
};

template <typename Node, avnd::soundfile_port Field, std::size_t N, std::size_t NField>
struct setup_control_for_exec<Node, Field, N, NField>
    : setup_control_for_exec_base<Node, Field>
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  void initialize_control(Field& param, Process::ControlInlet* inlet, int k)
  {
    // FIXME handle dynamic ports correctly
    // First we can load it directly since execution hasn't started yet
    if(auto hdl = loadSoundfile(inlet->value(), this->ctx.doc, this->ctx.execState))
      this->node_ptr->soundfile_loaded(
          hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }

  void connect_control_to_ui(Field& param, Process::ControlInlet* inlet, int k)
  {
    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    std::weak_ptr<ossia::execution_state> weak_st = this->ctx.execState;
    QObject::connect(
        inlet, &Process::ControlInlet::valueChanged, this->parent,
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
};

template <typename Node, avnd::midifile_port Field, std::size_t N, std::size_t NField>
struct setup_control_for_exec<Node, Field, N, NField>
    : setup_control_for_exec_base<Node, Field>
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  void initialize_control(Field& param, Process::ControlInlet* inlet, int k)
  {
    // FIXME handle dynamic ports correctly

    // First we can load it directly since execution hasn't started yet
    if(auto hdl = loadMidifile(inlet->value(), this->ctx.doc))
      this->node_ptr->midifile_loaded(
          hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }

  void connect_control_to_ui(Field& param, Process::ControlInlet* inlet, int k)
  {
    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    std::weak_ptr<ossia::execution_state> weak_st = this->ctx.execState;
    QObject::connect(
        inlet, &Process::ControlInlet::valueChanged, this->parent,
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
};

template <typename Node, avnd::raw_file_port Field, std::size_t N, std::size_t NField>
struct setup_control_for_exec<Node, Field, N, NField>
    : setup_control_for_exec_base<Node, Field>
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  static constexpr bool has_text = requires { decltype(Field::file)::text; };
  static constexpr bool has_mmap = requires { decltype(Field::file)::mmap; };

  void initialize_control(Field& param, Process::ControlInlet* inlet, int k)
  {
    // FIXME handle dynamic ports correctly

    // First we can load it directly since execution hasn't started yet
    if(auto hdl = loadRawfile(inlet->value(), this->ctx.doc, has_text, has_mmap))
    {
      if constexpr(avnd::port_can_process<Field>)
      {
        // FIXME also do it when we get a run-time message from the exec engine,
        // OSC, etc
        auto func = executePortPreprocess<Field>(*hdl);
        this->node_ptr->file_loaded(
            hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
        if(func)
          func(this->node_ptr->impl.effect);
      }
      else
      {
        this->node_ptr->file_loaded(
            hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
      }
    }
  }

  void connect_control_to_ui(Field& param, Process::ControlInlet* inlet, int k)
  {
    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    std::weak_ptr<ossia::execution_state> weak_st = this->ctx.execState;
    QObject::connect(
        inlet, &Process::ControlInlet::valueChanged, this->parent,
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
              n->file_loaded(f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
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
              n->file_loaded(f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
            });
          }
        }
    });
  }
};

template <typename Node>
struct dispatch_control_setup
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  Model& element;
  const Execution::Context& ctx;
  const std::shared_ptr<ExecNode>& node_ptr;
  QObject* parent;

  // Main function being invoked, which dispatches to all the actual implementations
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
        setup_control_for_exec<Node, Field, N, NField> setup{
            element, ctx, node_ptr, parent};

        setup.initialize_control(param, inlet, k);

        setup.invoke_update(param);

        setup.connect_control_to_ui(param, inlet, k);
      }
      k++;
    }
    // Else it's an unhandled value inlet
  }
};

}
