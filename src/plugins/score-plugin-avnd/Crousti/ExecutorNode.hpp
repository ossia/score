#pragma once
#include <Crousti/Concepts.hpp>
#include <Crousti/Metadatas.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/safe_nodes/tick_policies.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/apply_type.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/detail/for_each_in_tuple.hpp>
#include <ossia/dataflow/safe_nodes/node.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>
#include <bitset>

namespace ossia
{

template <class F, template<class...> class T1, class... T1s, template<class...> class T2, class... T2s>
void for_each_in_tuples(T1<T1s...>&& t1, T2<T2s...>& t2, F&& func)
{
  static_assert(sizeof...(T1s) == sizeof...(T2s));

  if constexpr(sizeof...(T1s) > 0)
  {
    using namespace std;
    for_each_in_tuples_impl(std::move(t1), t2, std::forward<F>(func), make_index_sequence<sizeof...(T1s)>());
  }
}

}
namespace oscr
{

template <typename T, typename = void>
struct has_event_policy : std::false_type
{
};
template <typename T>
struct has_event_policy<T, std::void_t<typename T::event_policy>>
    : std::true_type
{
};

template <typename T, typename = void>
struct has_audio_policy : std::false_type
{
};
template <typename T>
struct has_audio_policy<T, std::void_t<typename T::audio_policy>>
    : std::true_type
{
};

template <typename T, typename = void>
struct has_control_policy : std::false_type
{
};
template <typename T>
struct has_control_policy<T, std::void_t<typename T::control_policy>>
    : std::true_type
{
};

template<typename Exec_T>
struct InitInlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  void operator()(AudioInput auto& in, ossia::audio_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(MultichannelAudioInput<decltype(in)>) {
      in.samples.buffer = std::addressof(port->get());
    }
    else if constexpr(PortAudioInput<decltype(in)>) {
      in.port = std::addressof(*port);
    }
  }

  void operator()(PortValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(requires { in.is_event(); }) {
      port->is_event = in.is_event();
    }
    in.port = std::addressof(*port);
  }

  void operator()(TimedValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(requires { in.is_event(); }) {
      port->is_event = in.is_event();
    }
  }

  void operator()(SingleValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    if constexpr(requires { in.is_event(); }) {
      port->is_event = in.is_event();
    }
  }

  void operator()(PortMidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    in.port = std::addressof(*port);
  }
  void operator()(MessagesMidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  void operator()(TextureInput auto& in, ossia::texture_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  void operator()(ControlInput auto& ctrl, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    port->is_event = true;

    if constexpr(requires { ctrl.port; })
      ctrl.port = std::addressof(*port);

    get_control(ctrl).setup_exec(port);
  }
};

template<typename Exec_T>
struct InitOutlets
{
  Exec_T& self;
  ossia::outlets& outlets;
  void operator()(AudioOutput auto& out, ossia::audio_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(MultichannelAudioOutput<decltype(out)>) {
      out.samples.buffer = std::addressof(port->get());
    }
    else if constexpr(PortAudioOutput<decltype(out)>) {
      out.port = std::addressof(*port);
    }
  }

  void operator()(PortValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    out.port = std::addressof(*port);

    if constexpr(requires { out.type; }) {
      if(!out.type.empty())
        port->type = ossia::parse_dataspace(out.type);
    }
  }

  void operator()(TimedValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(requires { out.type; }) {
      if(!out.type.empty())
        port->type = ossia::parse_dataspace(out.type);
    }
  }

  void operator()(SingleValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(requires { out.type; }) {
      if(!out.type.empty())
        port->type = ossia::parse_dataspace(out.type);
    }
  }

  void operator()(MidiOutput auto& out, ossia::midi_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    out.port = std::addressof(*port);
  }

  void operator()(TextureOutput auto& out, ossia::texture_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  void operator()(ControlOutput auto& ctrl, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    if constexpr(requires { ctrl.port; })
      ctrl.port = std::addressof(*port);

    ctrl.display().setup_exec(port);
  }
};



template<typename Exec_T>
struct BeforeExecInlets
{
  Exec_T& self;
  const ossia::token_request& sub_tk;
  ossia::exec_state_facade st;


  void operator()(MultichannelAudioInput auto& in, ossia::audio_inlet& port) const noexcept
  {
    const auto [start, N] = st.timings(sub_tk);
    in.samples.offset = start;
    in.samples.duration = N;
  }

  void operator()(AudioEffectInput auto& in, ossia::audio_inlet& port) const noexcept
  {
    in.channels = port->channels();
    const auto [first_pos, N] = st.timings(sub_tk);

    // Allocate enough memory in our input buffers
    for (decltype(in.channels) i = 0; i < in.channels; i++)
    {
      auto& in = port->channel(i);
      if(int64_t(in.size()) - first_pos < N)
        in.resize(N + first_pos);
    }
  }

  void operator()(PortAudioInput auto& in, ossia::audio_inlet& port) const noexcept
  {
  }

  void operator()(PortValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
  }

  void operator()(TimedValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    using value_type = std::remove_reference_t<decltype(in.values[0])>;
    in.values.clear();
    if(const auto& inlet_values = port.data.get_data(); !inlet_values.empty())
    {
      const auto [start, dur] = st.timings(sub_tk);
      const auto end = start + dur;

      for(auto& [value, timestamp] : inlet_values)
      {
        if(timestamp >= start && timestamp < end)
        {
          // From within the node, the time base is reset to every sub-tick's start
          in.values[timestamp - start] = ossia::convert<value_type>(std::move(value));
        }
      }
    }
  }

  void operator()(SingleValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
    using value_type = std::remove_reference_t<decltype(in.value)>;
    if(auto& vec = port.data.get_data(); !vec.empty())
    {
      in.value = ossia::convert<value_type>(std::move(vec.back().value));
    }
  }

  void operator()(PortMidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
  }
  void operator()(MessagesMidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
    in.messages.clear();
    if(auto& inlet_values = port->messages; !inlet_values.empty())
    {
      const auto [start, dur] = st.timings(sub_tk);
      const auto end = start + dur;

      for(libremidi::message& msg : inlet_values)
      {
        if(msg.timestamp >= start && msg.timestamp < end)
        {
          // From within the node, the time base is reset to every sub-tick's start
          in.messages.push_back(std::move(msg));
          in.messages.back().timestamp -= start;
        }
      }
    }
  }

  void operator()(TextureInput auto& in, ossia::texture_inlet& port) const noexcept
  {
  }

  void operator()(ControlInput auto& ctrl, ossia::value_inlet& port) const noexcept
  {
  }
};

template<typename Exec_T>
struct BeforeExecOutlets
{
  Exec_T& self;
  const ossia::token_request& sub_tk;
  ossia::exec_state_facade st;

  void operator()(MultichannelAudioOutput auto& out, ossia::audio_outlet& port) const noexcept
  {
    const auto [start, N] = st.timings(sub_tk);
    out.samples.offset = start;
    out.samples.duration = N;
  }

  void operator()(AudioEffectOutput auto& out, ossia::audio_outlet& port) const noexcept
  {
  }

  void operator()(PortAudioOutput auto& out, ossia::audio_outlet& port) const noexcept
  {
  }

  void operator()(PortValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
  }

  void operator()(TimedValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
  }

  void operator()(SingleValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
  }

  void operator()(MidiOutput auto& out, ossia::midi_outlet& port) const noexcept
  {
  }

  void operator()(TextureOutput auto& out, ossia::texture_outlet& port) const noexcept
  {
  }

  void operator()(ControlOutput auto& ctrl, ossia::value_outlet& port) const noexcept
  {
  }
};

template<typename Exec_T>
struct AfterExecInlets
{
  Exec_T& self;
  const ossia::token_request& sub_tk;
  ossia::exec_state_facade st;

  void operator()(AudioInput auto& in, ossia::audio_inlet& port) const noexcept
  {
  }

  void operator()(PortValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
  }

  void operator()(TimedValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
  }

  void operator()(SingleValueInput auto& in, ossia::value_inlet& port) const noexcept
  {
  }

  void operator()(MidiInput auto& in, ossia::midi_inlet& port) const noexcept
  {
  }

  void operator()(TextureInput auto& in, ossia::texture_inlet& port) const noexcept
  {
  }

  void operator()(ControlInput auto& ctrl, ossia::value_inlet& port) const noexcept
  {
  }
};

template<typename Exec_T>
struct AfterExecOutlets
{
  Exec_T& self;
  const ossia::token_request& sub_tk;
  ossia::exec_state_facade st;

  void operator()(AudioOutput auto& out, ossia::audio_outlet& port) const noexcept
  {
  }

  void operator()(PortValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
  }

  void operator()(TimedValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    // We assume that the values won't be outside the bounds of the timestamp.
    for(auto& [timestamp, value] : out.values)
    {
      port->write_value(std::move(value), timestamp);
    }
    out.values.clear();
  }

  void operator()(SingleValueOutput auto& out, ossia::value_outlet& port) const noexcept
  {
    const auto [start, N] = st.timings(sub_tk);
    port->write_value(std::move(out.value), start);
  }


  void operator()(PortMidiOutput auto& out, ossia::midi_outlet& port) const noexcept
  {
  }
  void operator()(MessagesMidiOutput auto& out, ossia::midi_outlet& port) const noexcept
  {
    port->messages.insert(port->messages.end(), out.messages.begin(), out.messages.end());
    out.messages.clear();
  }

  void operator()(TextureOutput auto& out, ossia::texture_outlet& port) const noexcept
  {
  }

  void operator()(ControlOutput auto& ctrl, ossia::value_outlet& port) const noexcept
  {
  }
};

template <typename Node_T>
struct safe_node_inputs { };
template <typename Node_T>
requires DataflowNode<Node_T> && Inputs<Node_T> && HasControlInputs<Node_T>
struct safe_node_inputs<Node_T>
{
  using inlets_refl = inlet_reflection<Node_T>;
  // std::tuple<ossia::value_port, ossia::audio_port, ...>
  typename inlets_refl::ossia_inputs_tuple input_ports;

  // std::tuple<float, int...> : current running values of the controls
  using control_input_values_type = typename inlets_refl::control_input_values_type;
  control_input_values_type control_input;

  // bitset : 1 if the control has changed since the last tick, 0 else
  using control_input_changed_list = std::bitset<inlets_refl::control_in_count>;
  control_input_changed_list control_input_changed;

  // holds the std::tuple<timed_vec<float>, ...>
  using control_input_timed_t = typename ossia::apply_type<control_input_values_type, ossia::timed_vec>::type;
  control_input_timed_t control_input_timed;

  // used to communicate control changes from / to the ui
  ossia::spsc_queue<control_input_values_type> control_ins_queue;
};

template <typename Node_T>
requires DataflowNode<Node_T> && Inputs<Node_T> && (!HasControlInputs<Node_T>)
struct safe_node_inputs<Node_T>
{
  using inlets_refl = inlet_reflection<Node_T>;
  // std::tuple<ossia::value_port, ossia::audio_port, ...>
  typename inlets_refl::ossia_inputs_tuple input_ports;
};

template <typename Node_T>
struct safe_node_outputs { };
template <typename Node_T>
requires DataflowNode<Node_T> && Outputs<Node_T> && HasControlOutputs<Node_T>
struct safe_node_outputs<Node_T>
{
  using outlets_refl = outlet_reflection<Node_T>;

  // std::tuple<ossia::value_port, ossia::audio_port, ...>
  typename outlets_refl::ossia_outputs_tuple output_ports;

  // std::tuple<float, int...> : current running values of the controls
  using control_output_values_type = typename outlets_refl::control_output_values_type;
  control_output_values_type control_output;

  // bitset : 1 if the control has changed since the last tick, 0 else
  using control_output_changed_list = std::bitset<outlets_refl::control_in_count>;
  control_output_changed_list control_output_changed;

  // holds the std::tuple<timed_vec<float>, ...>
  using control_output_timed_t = typename ossia::apply_type<control_output_values_type, ossia::timed_vec>::type;
  control_output_timed_t control_output_timed;

  // used to communicate control changes from / to the ui
  ossia::spsc_queue<control_output_values_type> control_outs_queue;
};

template <typename Node_T>
requires DataflowNode<Node_T> && Outputs<Node_T> && (!HasControlOutputs<Node_T>)
struct safe_node_outputs<Node_T>
{
  using outlets_refl = outlet_reflection<Node_T>;

  // std::tuple<ossia::value_port, ossia::audio_port, ...>
  typename outlets_refl::ossia_outputs_tuple output_ports;
};

template <typename Node_T>
struct gfx_aspect { };

template <typename Node_T>
requires DataflowNode<Node_T> && (inlet_reflection<Node_T>::texture_in_count > 0 || outlet_reflection<Node_T>::texture_out_count > 0)
struct gfx_aspect<Node_T> {
  gfx_aspect()
  {

  }
  int32_t id{-1};
  std::atomic_int32_t script_index{0};
};


template <DataflowNode Node_T>
class safe_node final
    : public ossia::nonowning_graph_node
    , public safe_node_inputs<Node_T>
    , public safe_node_outputs<Node_T>
    , public gfx_aspect<Node_T>
{
public:
  Node_T state;

  using inlets_refl = inlet_reflection<Node_T>;
  using outlets_refl = outlet_reflection<Node_T>;

  safe_node(ossia::exec_state_facade st) noexcept
  {
    m_inlets.reserve(inlets_refl::inlet_size);
    m_outlets.reserve(outlets_refl::outlet_size);

    if constexpr(requires { state.inputs; })
    {
      InitInlets<safe_node> port_init_func{*this, this->m_inlets};
      ossia::for_each_in_tuples(boost::pfr::detail::tie_as_tuple(state.inputs), this->input_ports, port_init_func);
    }
    if constexpr(requires { state.outputs; })
    {
      InitOutlets<safe_node> port_init_func{*this, this->m_outlets};
      ossia::for_each_in_tuples(boost::pfr::detail::tie_as_tuple(state.outputs), this->output_ports, port_init_func);
    }

    // Initialize buffers if possible
    // FIXME
    if constexpr(requires { state.prepare(st); })
      state.prepare(st);
  }

  template <typename T, std::size_t ControlN>
  void control_updated_from_ui(T&& v)
  {
    using control_member = std::tuple_element_t<ControlN, typename inlets_refl::control_input_tuple>;
    if constexpr(requires { control_member{}.port; })
    {
      // using port_index_t = typename info::template control_input_index<ControlN>;
      // copy into the control's port directly
      // TODO does not work: this happens at the beginning of the entire tick.
      // but the input is cleared after that.
      // So we need to have some buffer for this case...
      // const auto& vp = get<port_index_t::value>(this->input_ports)->get_data();
    }
    else if constexpr(requires { control_member{}.values; })
    {
      // copy into the control's values array
      // NOTE: we do not use emplace / insert as we want to replace the existing value if any
      std::get<ControlN>(this->control_input_timed)[int64_t{0}] = std::move(v);
    }
    else if constexpr(requires { control_member{}.value; })
    {
      if constexpr(std::is_same_v<T, ossia::impulse>)
      {
        std::get<ControlN>(this->control_input_timed)[int64_t{0}] = std::move(v);
      }
      else
      {
        std::get<ControlN>(this->control_input) = std::move(v);
      }
    }
  }

  // Loads the control data from the port, into the timed input.
  template <std::size_t ControlN>
  constexpr const auto& get_control_accessor() noexcept
  {
    using namespace boost::pfr;
    using namespace std;
    using control_member = std::tuple_element_t<ControlN, typename inlets_refl::control_input_tuple>;
    constexpr auto control = get_control<control_member>();
    using control_type = decltype(control);
    using control_value_type = typename control_type::type;

    // ControlN = 0: first control in this->controls, this->control_tuple, etc..

    // Used to index into the set of input_ports
    using port_index_t = typename inlets_refl::template control_input_index<ControlN>;

    // Get the timed_vector<float>
    auto& vec = get<ControlN>(this->control_input_timed);
    // vec.clear();

    // Get the ossia::value_port data
    const auto& vp = get<port_index_t::value>(this->input_ports)->get_data();
    vec.container.reserve(vp.size() + 1);

    if constexpr(std::is_same_v<control_value_type, ossia::impulse>)
    {
      static_assert(TimedVec<decltype(control_member::values)>, "impulses only make sense as an array of timed values");
      // copy all the values... values arrived later replace previous ones
      load_control_from_port<control_type::must_validate, ControlN, port_index_t::value>(vec, vp);
    }
    else
    {
      // in all cases, set the current value at t=0 unless it was already set
      vec.emplace(int64_t{0}, get<ControlN>(this->control_input));

      // copy all the values... values arrived later replace previous ones
      load_control_from_port<control_type::must_validate, ControlN, port_index_t::value>(vec, vp);

      // the last value will be the first for the next tick
      get<ControlN>(this->control_input) = vec.rbegin()->second;
    }

    return vec;
  }

  template <std::size_t ControlN>
  constexpr auto& get_control_outlet_accessor (const ossia::outlets& outl) noexcept
  {
    static_assert(outlets_refl::control_out_count > 0);
    static_assert(ControlN < outlets_refl::control_out_count);

    return std::get<ControlN>(this->control_outs_tuple);
  }

  template <bool Validate, std::size_t ControlN, std::size_t PortN, typename Vec, typename Vp>
  void load_control_from_port(Vec& vec, const Vp& vp) noexcept
  {
    using namespace boost::pfr;
    using control_member = std::remove_reference_t<decltype(get<PortN>(state.inputs))>;
    constexpr const auto ctrl = get_control<control_member>();
    if constexpr(requires { ctrl.fromValue(ossia::value{}, vec[0]); })
    {
      // New API, allows the widgets to overload on different value types
      // (e.g. to allow enums to work with, int, string, enum...
      if constexpr(Validate)
      {
        for (auto& v : vp)
        {
          std::remove_reference_t<decltype(vec[0])> val;
          if (ctrl.fromValue(v.value, val))
          {
            vec[int64_t{v.timestamp}] = std::move(val);
            this->control_input_changed.set(ControlN);
          }
        }
      }
      else
      {
        for (auto& v : vp)
        {
          ctrl.fromValue(v.value, vec[int64_t{v.timestamp}]);
          this->control_input_changed.set(ControlN);
        }
      }
    }
    else
    {
      // Old API, deprecated
      if constexpr(Validate)
      {
        for (auto& v : vp)
        {
          if (auto res = ctrl.fromValue(v.value))
          {
            vec[int64_t{v.timestamp}] = *std::move(res);
            this->control_input_changed.set(ControlN);
          }
        }
      }
      else
      {
        for (auto& v : vp)
        {
          vec[int64_t{v.timestamp}] = ctrl.fromValue(v.value);
          this->control_input_changed.set(ControlN);
        }
      }
    }
  }

  template<typename Port, typename Control>
  void apply_control_impl(Port& port, Control&& ctl)
  {
    if constexpr(requires { port.value; })
    {
      // control has e.g. a float value; : copy the running value in it
      port.value = std::forward<Control>(ctl);
    }
    if constexpr(requires { port.values; })
    {
      // nothing to do, already copied
      if constexpr(requires { get_control(port).convert(std::forward<Control>(ctl), port.values); })
        get_control(port).convert(std::forward<Control>(ctl), port.values);
      else
        port.values = std::forward<Control>(ctl);
    }
    else if constexpr(requires { port.port; })
    {
      // nothing to do, already set
    }
  }

  // copies all the controls to the state class
  template<typename... Controls, std::size_t... CI>
  void apply_controls_impl(const std::index_sequence<CI...>&, Controls&&... ctls)
  {
    using namespace boost::pfr;
    (apply_control_impl(get<inlets_refl::template control_input_index<CI>::value>(state.inputs), ctls),
     ...);
  }

  constexpr auto get_policy() noexcept {

    if constexpr(requires { std::is_constructible_v<typename Node_T::control_policy>; }) {
      return typename Node_T::control_policy{};
    }
    else {
      if constexpr(boost::mp11::mp_any_of<typename inlets_refl::control_input_tuple, uses_timed_values>::value)
        return typename ossia::safe_nodes::default_tick{};
      else
        return typename ossia::safe_nodes::last_tick{};
    }
  }

  constexpr std::size_t audio_output_channels(AudioEffectOutput auto& field)
  {
    if constexpr(requires { field.want_channels(); }) {
      // Fixed number of channels
      return field.want_channels();
    }
    else if constexpr(requires { field.mimic_channels(); }) {
      // Dynamic: uses the same amount of channels than an input
      return (state.inputs.*(field.mimic_channels())).channels;
    } else if constexpr(AudioEffectInput<decltype(boost::pfr::get<0>(state.inputs))>) {
      return boost::pfr::get<0>(state.inputs).channels;
    } else {
      static_assert(field.want_channels() or field.mimic_channels(), "aren't implemented");
      return 0;
    }
  }

  void do_run(const ossia::token_request& sub_tk, ossia::exec_state_facade st)
  {
    /// Prepare inlets and outlets ///
    if constexpr(Inputs<Node_T>)
    {
      ossia::for_each_in_tuples(
          boost::pfr::detail::tie_as_tuple(state.inputs),
          this->input_ports,
          BeforeExecInlets<safe_node>{*this, sub_tk, st});
    }
    if constexpr(Outputs<Node_T>)
    {
      ossia::for_each_in_tuples(
          boost::pfr::detail::tie_as_tuple(state.outputs),
          this->output_ports,
          BeforeExecOutlets<safe_node>{*this, sub_tk, st});
    }

    /// Prepare samples for "raw" audio channels.
    const auto timings = st.timings(sub_tk);

    {
      // 1. Count inputs
      if constexpr(Inputs<Node_T>)
      {
        int total_input_channels{};
        const double** input_channel_data{};

        // First loop to count all the inputs channels used in order to do an alloca
        avnd::for_each_field_ref(
            this->state.inputs,
            [&] <typename T> (T&& field) {
              if constexpr(AudioEffectInput<T>) {
                total_input_channels += field.channels;
              }
            });

        // Do the alloca here (it has to be in the function that will call "state")
        input_channel_data = (const double**)alloca(sizeof(double*) * total_input_channels);

        const double** channel_data_ref = input_channel_data;

        // Now assign the pointers in order by incrementing by the number of channels.
        // Memory is already allocated by BeforeExecInlets.
        ossia::for_each_in_tuples(
            boost::pfr::detail::tie_as_tuple(state.inputs),
            this->input_ports,
            [&] <typename T> (T&& field, auto& port) {
              if constexpr(AudioEffectInput<T>) {
                field.samples = channel_data_ref;
                for(decltype(field.channels) i = 0; i < field.channels; ++i)
                {
                  auto& ossia_channel = port.data.channel(i);
                  const int64_t available_samples = ossia_channel.size();
                  if(available_samples - timings.start_sample < timings.length)
                    ossia_channel.resize(timings.length + timings.start_sample);
                  field.samples[i] = ossia_channel.data() + timings.start_sample;
                }
                channel_data_ref += field.channels;
              }
            });
      }

      if constexpr(Outputs<Node_T>)
      {
        int total_output_channels = 0;
        double** output_channel_data{};
        avnd::for_each_field_ref(
            this->state.outputs,
            [&] <typename T> (T&& field) {
              if constexpr(AudioEffectOutput<T>) {
                total_output_channels += audio_output_channels(field);
              }
            });

        // Do the alloca here (it has to be in the function that will call "state")
        output_channel_data = (double**)alloca(sizeof(double*) * total_output_channels);

        double** channel_data_ref = output_channel_data;

        // Now assign the pointers in order by incrementing by the number of channels.
        // Memory is already allocated by BeforeExecInlets.
        ossia::for_each_in_tuples(
            boost::pfr::detail::tie_as_tuple(state.outputs),
            this->output_ports,
            [&] <typename T> (T&& field, auto& port) {
              if constexpr(AudioEffectOutput<T>) {
                field.samples = channel_data_ref;

                const std::size_t channels = audio_output_channels(field);
                port.data.set_channels(channels);
                for(std::size_t i = 0; i < channels; ++i)
                {
                  auto& ossia_channel = port.data.channel(i);
                  const int64_t available_samples = ossia_channel.size();
                  if(available_samples - timings.start_sample < timings.length)
                    ossia_channel.resize(timings.length + timings.start_sample);
                  field.samples[i] = ossia_channel.data() + timings.start_sample;
                }
                channel_data_ref += channels;
              }
            });
      }
    }

    // Execute depending on what we have
    if constexpr(RunnableWithTokenRequest<Node_T>)
    {
      state(sub_tk, st);
    }
    else if constexpr(RunnableWithSampleCount<Node_T>)
    {
      state(timings.length);
    }
    else if constexpr(RunnableWithoutArguments<Node_T>)
    {
      state();
    }

    /// Finish inlets and outlets ///
    if constexpr(requires { state.inputs; })
    {
      ossia::for_each_in_tuples(
          boost::pfr::detail::tie_as_tuple(state.inputs),
          this->input_ports,
          AfterExecInlets<safe_node>{*this, sub_tk, st});
    }
    if constexpr(requires { state.outputs; })
    {
      ossia::for_each_in_tuples(
          boost::pfr::detail::tie_as_tuple(state.outputs),
          this->output_ports,
          AfterExecOutlets<safe_node>{*this, sub_tk, st});
    }
  }

  template <std::size_t... CI>
  void apply_all_impl(
      const std::index_sequence<CI...>&,
      const ossia::token_request& tk,
      ossia::exec_state_facade st) noexcept
  {
    static_assert(inlets_refl::control_in_count > 0);

    get_policy()([&] (const ossia::token_request& sub_tk, auto&& ... ctls) {
                   apply_controls_impl(std::make_index_sequence<sizeof... (ctls)>{}, ctls...);
                   do_run(sub_tk, st);
                 }, tk, get_control_accessor<CI>()...);
  }

  void clear_controls_in()
  {
    ossia::for_each_in_tuple(this->control_input_timed, [] (auto& vec) { vec.clear(); });
  }

  void clear_controls_out()
  {
    ossia::for_each_in_tuple(this->control_output_timed, [] (auto& vec) { vec.clear(); });
  }

  void
  run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    /// General setup ///
    if constexpr(outlets_refl::control_out_count > 0)
    {
      clear_controls_out();
    }

    /// Execution ///
    if constexpr (inlets_refl::control_in_count == 0)
    {
      // If there are no controls
      do_run(tk, st);
    }
    else
    {
      // If there are controls
      using controls_indices = std::make_index_sequence<inlets_refl::control_in_count>;

      apply_all_impl(controls_indices{}, tk, st);

      if (this->control_ins_queue.size_approx() < 1 && this->control_input_changed.any())
      {
        this->control_ins_queue.try_enqueue(this->control_input);
        this->control_input_changed.reset();
      }

      clear_controls_in();
    }

    /// Post-execution tasks ///
    if constexpr(outlets_refl::control_out_count > 0)
    {
      std::size_t i = 0;
      bool ok = false;

      ossia::for_each_in_tuples_ref(
          this->control_output_timed,
          this->control_output,
          [&] (auto&& control_in, auto&& control_out) {
            if(!control_in.empty())
            {
              ok = true;
              control_out = std::move(control_in.container.back().second);
              control_in.clear();
            }

            i++;
      });

      if(ok)
      {
        this->control_outs_queue.enqueue(this->control_output);
      }
    }
  }

  void all_notes_off() noexcept override
  {
    if constexpr (inlets_refl::midi_in_count > 0)
    {
      // TODO
    }
  }

  std::string label() const noexcept override
  {
    return "Control";
  }
};
}
