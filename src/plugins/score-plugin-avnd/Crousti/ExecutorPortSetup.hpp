#pragma once
#include <Process/Process.hpp>

#include <Crousti/File.hpp>
#include <Crousti/ProcessModel.hpp>

#include <ossia/detail/flat_map.hpp>

#include <avnd/binding/ossia/node.hpp>

namespace oscr
{
template <typename T>
struct dynamic_ports_component_data
{
};

//! State the executor of a process with dynamic ports keeps between two
//! recompute_ports(): the model ports its exec node was last set up for, and
//! the control inlets whose UI -> exec connection is already made (a resize
//! keeps the surviving ports and only the new ones need to be connected).
template <oscr::has_dynamic_ports T>
struct dynamic_ports_component_data<T>
{
  Process::Inlets m_oldInlets;
  Process::Outlets m_oldOutlets;
  ossia::flat_map<Process::Inlet*, std::pair<int, QMetaObject::Connection>>
      m_connectedControls;
};

// A halp::folder_port: a std::string control whose widget is a directory picker
// (`enum widget { folder };`). Like the file ports, its value is a path and must
// go through score::locateFilePath so that <LIBRARY>:, <PROJECT>: and
// document-relative paths are resolved before the object reads it.
template <typename T>
concept folder_control_port = requires { T::folder; } && requires(T t) {
  { t.value } -> std::convertible_to<std::string_view>;
};

template <typename Node, typename Field, std::size_t NPred, std::size_t NField>
struct con_unvalidated
{
  using ExecNode = safe_node<Node>;
  const Execution::Context& ctx;
  std::weak_ptr<ExecNode> weak_node;
  Field& field;

  // NPred is the index of the port in whichever list it was dispatched from in
  // Executor::connect_controls -- the controls, but also e.g. the curve ports.
  // control_updated_from_ui indexes the control list, so the two only coincide
  // when the port happens to be at the same position in both. Recompute it from
  // the field index, which is unambiguous.
  static constexpr std::size_t control_index
      = avnd::control_input_introspection<Node>::field_index_to_index(
          avnd::field_index<NField>{});

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
          n->template control_updated_from_ui<control_value_type, control_index>(
              std::move(v));
        }
      });
    }
  }
};

//! UI -> exec for one port of a dynamic port group.
//! Field is the group (the halp::dynamic_port), NPred its index among the
//! dynamic ports, port_index the port within the group.
template <typename Node, typename Field, std::size_t NPred, std::size_t NField>
struct con_unvalidated_dynamic_port
{
  using ExecNode = safe_node<Node>;
  const Execution::Context& ctx;
  std::weak_ptr<ExecNode> weak_node;
  int port_index;

  void operator()(const ossia::value& val)
  {
    using control_type = avnd::dynamic_port_type<Field>;
    using control_value_type = std::decay_t<decltype(control_type::value)>;

    if(auto node = weak_node.lock())
    {
      // The group's port vector belongs to the execution thread, which resizes
      // it: it must not be read from here, not even to look up the port. The
      // conversion only depends on the port type, so it is done against a
      // local instance; the bounds check happens in the execution thread,
      // in control_updated_from_ui.
      control_type witness{};
      control_value_type v;
      node->from_ossia_value(witness, val, v, avnd::field_index<NField>{});
      ctx.executionQueue.enqueue([weak_node = weak_node, port_index = port_index,
                                  v = std::move(v)]() mutable {
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

  void invoke_update(Field& param, int k)
  {
    avnd::effect_container<Node>& eff = node_ptr->impl;
    {
      for(auto state : eff.full_state())
      {
        if constexpr(avnd::dynamic_ports_port<Field>)
        {
          if_possible(param.ports[k].update(state.effect));
        }
        else
        {
          if_possible(param.update(state.effect));
        }
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
      using port_type = avnd::dynamic_port_type<Field>;
      if constexpr(avnd::parameter_port<port_type>)
      {
        if constexpr(!requires { param.ports[0].value.reset(); })
        {
          this->node_ptr->from_ossia_value(
              param, inlet->value(), param.ports[k].value, avnd::field_index<NField>{});
        }
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
        ossia::qt::run_async(qApp, [self, inlet, val = std::move(value)] {
          if(!self || !inlet)
            return;

          // FIXME better to use in_edit queue ?
          // FIXME not too efficient but which choice do we have ?
          static const Field field;
          oscr::to_ossia_value(field, val);
          inlet->setValue(val);
        });
      };
    }
  }

  //! Connects inlet -> exec unless it already is.
  //! Returns whether a connection was made. Used both at creation and when
  //! ports are added by a resize.
  bool reconnect_control_to_ui(
      dynamic_ports_component_data<Node>& control_data, Field& param,
      Process::ControlInlet* inlet, int k)
  {
    if constexpr(requires { control_data.m_connectedControls; })
    {
      auto it = control_data.m_connectedControls.find(inlet);
      if(it != control_data.m_connectedControls.end())
      {
        if constexpr(!avnd::dynamic_ports_port<Field>)
          return false;
        if(it->second.first == k)
          return false;
        QObject::disconnect(it->second.second);
        control_data.m_connectedControls.erase(it);
      }
    }

    // Connect to changes
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    QMetaObject::Connection connection;
    if constexpr(avnd::dynamic_ports_port<Field>)
    {
      connection = QObject::connect(
          inlet, &Process::ControlInlet::valueChanged, this->parent,
          con_unvalidated_dynamic_port<Node, Field, N, NField>{this->ctx, weak_node, k});
    }
    else
    {
      connection = QObject::connect(
          inlet, &Process::ControlInlet::valueChanged, this->parent,
          con_unvalidated<Node, Field, N, NField>{this->ctx, weak_node, param});
    }

    if constexpr(requires { control_data.m_connectedControls; })
      control_data.m_connectedControls.emplace(inlet, std::pair{k, connection});
    return true;
  }

  //! Sends the current value of the inlet to the exec node, the way a change
  //! from the UI would. For a port that appears during execution: nobody
  //! initialized the exec side with its value.
  void push_current_value(Field& param, Process::ControlInlet* inlet, int k)
  {
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    if constexpr(avnd::dynamic_ports_port<Field>)
    {
      con_unvalidated_dynamic_port<Node, Field, N, NField>{this->ctx, weak_node, k}(
          inlet->value());
    }
    else
    {
      con_unvalidated<Node, Field, N, NField>{this->ctx, weak_node, param}(
          inlet->value());
    }
  }

  // Used on initial creation
  void connect_control_to_ui(
      dynamic_ports_component_data<Node>& control_data, Field& param,
      Process::ControlInlet* inlet, int k)
  {
    reconnect_control_to_ui(control_data, param, inlet, k);
    this->update_controller(param, inlet);
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

  void connect_control_to_ui(
      dynamic_ports_component_data<Node>&, Field& param, Process::ControlInlet* inlet,
      int k)
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
              f = n->soundfile_loaded(
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

  void connect_control_to_ui(
      dynamic_ports_component_data<Node>&, Field& param, Process::ControlInlet* inlet,
      int k)
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
            f = n->midifile_loaded(
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

  void connect_control_to_ui(
      dynamic_ports_component_data<Node>&, Field& param, Process::ControlInlet* inlet,
      int k)
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
              f = n->file_loaded(
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
              f = n->file_loaded(
                  f, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
            });
          }
        }
    });
  }
};

// folder_port: a path-valued string control. Resolve <LIBRARY>: / <PROJECT>: /
// document-relative paths through score::locateFilePath before the object reads
// them -- exactly like the file ports above -- otherwise the object receives the
// raw library-relative string and cannot find the directory.
template <typename Node, folder_control_port Field, std::size_t N, std::size_t NField>
struct setup_control_for_exec<Node, Field, N, NField>
    : setup_control_for_exec_base<Node, Field>
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  void initialize_control(Field& param, Process::ControlInlet* inlet, int k)
  {
    if constexpr(!requires { param.value.reset(); })
    {
      this->node_ptr->from_ossia_value(
          param, resolvePathValue(inlet->value(), this->ctx.doc), param.value,
          avnd::field_index<NField>{});
    }
  }

  void connect_control_to_ui(
      dynamic_ports_component_data<Node>&, Field& param, Process::ControlInlet* inlet,
      int k)
  {
    std::weak_ptr<ExecNode> weak_node = this->node_ptr;
    QObject::connect(
        inlet, &Process::ControlInlet::valueChanged, this->parent,
        [&ctx = this->ctx, weak_node = std::move(weak_node),
         field = &param](const ossia::value& val) {
      // Resolve the path on every change, then run the normal control-update
      // path (from_ossia_value + control_updated_from_ui) via con_unvalidated.
      con_unvalidated<Node, Field, N, NField>{ctx, weak_node, *field}(
          resolvePathValue(val, ctx.doc));
    });
  }
};

//! Whether the model ports of this field are control inlets that drive the
//! object from the UI. A dynamic group of e.g. audio or message ports has
//! nothing to set up here (and no `value` to convert to).
template <typename Field>
constexpr bool field_has_ui_controls
    = !ossia_port<avnd::concrete_port_type<Field>>
      && (!avnd::dynamic_ports_port<Field>
          || avnd::parameter_port<avnd::concrete_port_type<Field>>);

template <typename Node>
struct dispatch_control_setup
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  Model& element;
  const Execution::Context& ctx;
  const std::shared_ptr<ExecNode>& node_ptr;
  dynamic_ports_component_data<Node>& control_data;
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

    if constexpr(field_has_ui_controls<Field>)
    {
      int k = 0;
      for(auto p : ports)
      {
        if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
        {
          setup_control_for_exec<Node, Field, N, NField> setup{
              element, ctx, node_ptr, parent};

          setup.initialize_control(param, inlet, k);

          setup.invoke_update(param, k);

          setup.connect_control_to_ui(control_data, param, inlet, k);
        }
        k++;
        // Else it's an unhandled value inlet
      }
    }
  }
};

// Only used when dynamic ports are added
template <typename Node>
struct dispatch_control_reconnect
{
  using ExecNode = safe_node<Node>;
  using Model = ProcessModel<Node>;

  Model& element;
  const Execution::Context& ctx;
  const std::shared_ptr<ExecNode>& node_ptr;
  dynamic_ports_component_data<Node>& control_data;
  QObject* parent;

  // Main function being invoked, which dispatches to all the actual implementations
  template <typename Field, std::size_t N, std::size_t NField>
  constexpr void
  operator()(Field& param, avnd::predicate_index<N> np, avnd::field_index<NField> nf)
  {
    if constexpr(field_has_ui_controls<Field>)
    {
      const auto ports = element.avnd_input_idx_to_model_ports(NField);
      int k = 0;
      for(auto p : ports)
      {
        if(auto inlet = qobject_cast<Process::ControlInlet*>(p))
        {
          setup_control_for_exec<Node, Field, N, NField> setup{
              element, ctx, node_ptr, parent};

          // A port that was just added: connect it and give the exec side its
          // current value. Ports that were already there keep their connection
          // and their value.
          if(setup.reconnect_control_to_ui(control_data, param, inlet, k))
            setup.push_current_value(param, inlet, k);
        }
        k++;
        // Else it's an unhandled value inlet
      }
    }
  }
};
}
