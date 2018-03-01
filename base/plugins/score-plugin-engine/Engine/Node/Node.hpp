#pragma once
#include <Engine/Node/Port.hpp>
#include <boost/container/flat_map.hpp>
#include <brigand/sequences/list.hpp>
#include <brigand/algorithms/index_of.hpp>
#include <brigand/algorithms/transform.hpp>
#include <ossia/detail/algorithms.hpp>
#include <tuple>
#include <array>

namespace Control
{

template<typename T>
using timed_vec = boost::container::flat_map<int64_t, T>;


template<typename... Args>
using NodeInfo = std::tuple<Args...>;

template<typename... Args>
static constexpr auto make_node(Args&&... args)
{
  return NodeInfo<Args...>{std::forward<Args>(args)...};
}

template<std::size_t N>
using AddressIns = std::array<AudioInInfo, N>;
template<std::size_t N>
using AudioIns = std::array<AudioInInfo, N>;
template<std::size_t N>
using AudioOuts = std::array<AudioOutInfo, N>;
template<std::size_t N>
using MidiIns = std::array<MidiInInfo, N>;
template<std::size_t N>
using MidiOuts = std::array<MidiOutInfo, N>;
template<std::size_t N>
using ValueIns = std::array<ValueInInfo, N>;
template<std::size_t N>
using ValueOuts = std::array<ValueOutInfo, N>;
template<std::size_t N>
using Controls = std::array<ControlInfo, N>;

template<typename T>
struct StateWrapper
{
    using state_type = T;
};
template<typename... Args>
struct NodeBuilder: Args...
{
  constexpr auto audio_ins() const { return *this; }
  constexpr auto audio_outs() const { return *this; }
  constexpr auto midi_ins() const { return *this; }
  constexpr auto midi_outs() const { return *this; }
  constexpr auto value_ins() const { return *this; }
  constexpr auto value_outs() const { return *this; }
  constexpr auto controls() const { return *this; }

  template<typename... SArgs>
  constexpr NodeBuilder(SArgs&&... sargs): Args{sargs}... { }

  template<std::size_t N>
  constexpr auto address_ins(const AddressInInfo (&arg)[N]) const {
    return NodeBuilder<std::array<AddressInInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }
  template<std::size_t N>
  constexpr auto audio_ins(const AudioInInfo (&arg)[N]) const {
    return NodeBuilder<std::array<AudioInInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }
  template<std::size_t N>
  constexpr auto audio_outs(const AudioOutInfo (&arg)[N]) const {
    return NodeBuilder<std::array<AudioOutInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }

  template<std::size_t N>
  constexpr auto midi_ins(const MidiInInfo (&arg)[N]) const {
    return NodeBuilder<std::array<MidiInInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }
  template<std::size_t N>
  constexpr auto midi_outs(const MidiOutInfo (&arg)[N]) const {
    return NodeBuilder<std::array<MidiOutInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }

  template<std::size_t N>
  constexpr auto value_ins(const ValueInInfo (&arg)[N]) const {
    return NodeBuilder<std::array<ValueInInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }
  template<std::size_t N>
  constexpr auto value_outs(const ValueOutInfo (&arg)[N]) const {
    return NodeBuilder<std::array<ValueOutInfo, N>, Args...>{ossia::to_array(arg), static_cast<Args>(*this)...};
  }
  template<typename... Controls>
  constexpr auto controls(Controls&&... ctrls) const {
    return NodeBuilder<std::tuple<Controls...>, Args...>{
        std::make_tuple(ctrls...), static_cast<Args>(*this)...
    };
  }

  constexpr auto build() const {
    return NodeInfo<Args...>{static_cast<Args>(*this)...};
  }
};

constexpr auto create_node() {
  return NodeBuilder<>{};
}


template<typename T, typename...>
struct is_port : std::false_type {};
template<typename T, std::size_t N>
struct is_port<T, std::array<T, N>> : std::true_type {};

template<typename T>
struct dummy_container {
    static constexpr bool is_event = false;
    static constexpr auto begin() { return (T*)nullptr; }
    static constexpr auto end() { return (T*)nullptr; }
    static constexpr std::size_t size() { return 0; }
};


template<typename PortType, typename T>
static constexpr auto get_ports(const T& t)
{
  using index = brigand::index_if<T, is_port<PortType, brigand::_1>>;

  if constexpr(!std::is_same<index, brigand::no_such_type_>::value)
  {
    using array_type = brigand::at<T, index>;

    return std::get<array_type>(t);
  }
  else
  {
    return dummy_container<PortType>{};
  }
}

template<typename...>
struct is_controls : std::false_type {};
template<typename... Args>
struct is_controls <std::tuple<Args...>> : std::true_type {};

template<typename T>
static constexpr auto get_controls(const T& t)
{
  using index = brigand::index_if<T, is_controls<brigand::_1>>;

  if constexpr(!std::is_same<index, brigand::no_such_type_>::value)
  {
    using tuple_type = brigand::at<T, index>;

    return std::get<tuple_type>(t);
  }
  else
  {
    return std::tuple<>{};
  }
}

struct dummy_t { };

template<typename T>
using my_void_t = void;
template<typename T, typename = void>
struct has_state_t : std::false_type{};
template<typename T>
struct has_state_t<T, my_void_t<typename T::State>> : std::true_type{};


template<typename T, typename = void>
struct get_state { using type = dummy_t; };
template<typename T>
struct get_state<T, my_void_t<typename T::State>> { using type = typename T::State; };

static constexpr bool same(QLatin1String s1, QLatin1String s2)
{
  if(s1.size() != s2.size())
    return false;
  for(int i = 0; i < s1.size(); i++)
  {
    if(s1[i] != s2[i])
      return false;
  }
  return true;
}
template<typename T>
struct get_control_type
{
    using type = typename T::type;
};
template<typename Info>
struct InfoFunctions
{
  using controls_type = decltype(get_controls(Info::info));
  using controls_values_type = brigand::transform<controls_type, get_control_type<brigand::_1>>;

  static constexpr auto audio_in_count =
      get_ports<AudioInInfo>(Info::info).size();
  static constexpr auto audio_out_count =
      get_ports<AudioOutInfo>(Info::info).size();
  static constexpr auto midi_in_count =
      get_ports<MidiInInfo>(Info::info).size();
  static constexpr auto midi_out_count =
      get_ports<MidiOutInfo>(Info::info).size();
  static constexpr auto value_in_count =
      get_ports<ValueInInfo>(Info::info).size();
  static constexpr auto value_out_count =
      get_ports<ValueOutInfo>(Info::info).size();
  static constexpr auto control_count =
      std::tuple_size<controls_type>::value;
  static constexpr auto address_in_count =
      get_ports<AddressInInfo>(Info::info).size();

  static constexpr auto categorize_inlet(std::size_t i)
  {
    if(i < audio_in_count)
      return inlet_kind::audio_in;
    else if(i < audio_in_count + midi_in_count)
      return inlet_kind::midi_in;
    else if(i < audio_in_count + midi_in_count + value_in_count)
      return inlet_kind::value_in;
    else if(i < audio_in_count + midi_in_count + value_in_count + address_in_count)
      return inlet_kind::address_in;
    else if(i < audio_in_count + midi_in_count + value_in_count + address_in_count + control_count)
      return inlet_kind::control_in;
    else
      throw std::runtime_error("Invalid input number");
  }


  static constexpr auto control_start =
      audio_in_count + midi_in_count + value_in_count + address_in_count;

  static constexpr auto inlet_size =
      control_start + control_count;

  static constexpr auto outlet_size =
      audio_out_count + midi_out_count + value_out_count;

};

}
