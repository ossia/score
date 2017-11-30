#pragma once
#include <Engine/Node/Port.hpp>
#include <boost/container/flat_map.hpp>
#include <brigand/sequences/list.hpp>
#include <brigand/algorithms/index_of.hpp>
#include <ossia/detail/algorithms.hpp>
#include <tuple>
#include <array>

namespace Process
{

template<typename T>
using timed_vec = boost::container::flat_map<int64_t, T>;



template<typename... Args>
struct NodeInfo: Args...
{
  using types = brigand::list<Args...>;
};

template<typename... Args>
static constexpr auto make_node(Args&&... args)
{
  return NodeInfo<Args...>{std::forward<Args>(args)...};
}

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
    using type = T;
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
  constexpr auto controls(const Controls&&... ctrls) const {
    return NodeBuilder<std::tuple<Controls...>, Args...>{
        std::make_tuple(ctrls...), static_cast<Args>(*this)...
    };
  }
  template<typename T>
  constexpr auto state() const {
    return NodeBuilder<StateWrapper<T>, Args...>{
      StateWrapper<T>{}, static_cast<Args>(*this)...
    };
  }
  constexpr auto build() const {
    return NodeInfo<Args...>{static_cast<Args>(*this)...};
  }
};

constexpr auto create_node() {
  return NodeBuilder{};
}


template<typename T, typename...>
struct is_port : std::false_type {};
template<typename T, std::size_t N>
struct is_port<T, std::array<T, N>> : std::true_type {};

template<typename PortType, typename T>
static constexpr auto get_ports(const T& t)
{
  using index = brigand::index_if<typename T::types, is_port<PortType, brigand::_1>>;

  if constexpr(!std::is_same<index, brigand::no_such_type_>::value)
  {
    using array_type = brigand::at<typename T::types, index>;
    return static_cast<const array_type&>(t);
  }
  else
  {
    return std::array<PortType, 0>{};
  }
}

template<typename...>
struct is_controls : std::false_type {};
template<typename... Args>
struct is_controls <std::tuple<Args...>> : std::true_type {};

template<typename T>
static constexpr auto get_controls(const T& t)
{
  using index = brigand::index_if<typename T::types, is_controls<brigand::_1>>;

  if constexpr(!std::is_same<index, brigand::no_such_type_>::value)
  {
    using tuple_type = brigand::at<typename T::types, index>;
    return static_cast<const tuple_type&>(t);
  }
  else
  {
    return std::tuple<>{};
  }
}

template<typename...>
struct is_state : std::false_type {};
template<typename T>
struct is_state<StateWrapper<T>> : std::true_type {};
struct dummy_t { };
template<typename T>
static constexpr auto get_state(const T& t)
{
  using index = brigand::index_if<typename T::types, is_state<brigand::_1>>;

  if constexpr(!std::is_same<index, brigand::no_such_type_>::value)
  {
    using type = brigand::at<typename T::types, index>;
    return typename type::type{};
  }
  else
  {
    return dummy_t{};
  }
}
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
template<typename Info>
struct InfoFunctions
{



  /*
  template<std::size_t N>
  static constexpr std::size_t inlet(const char (&n)[N])
  {
    QLatin1String name(n, N);
    std::size_t i = 0;
    for(const auto& in : get_ports<AudioInInfo>(Info::info))
    {
      if (same(in.name, name))
        return i;
      i++;
    }
    for(const auto& in : get_ports<MidiInInfo>(Info::info))
    {
      if (same(in.name, name))
        return i;
      i++;
    }
    for(const auto& in : get_ports<ValueInInfo>(Info::info))
    {
      if (same(in.name, name))
        return i;
      i++;
    }
    for(const auto& in : get_ports<ControlInfo>(Info::info))
    {
      if (same(in.name, name))
        return i;
      i++;
    }
    throw;
  }


  template<std::size_t N>
  static constexpr std::size_t outlet(const char (&n)[N])
  {
    QLatin1String name(n, N);
    std::size_t i = 0;
    for(const auto& in : get_ports<AudioOutInfo>(Info::info))
      if (same(in.name, name))
        return i;
      else
        i++;

    for(const auto& in : get_ports<MidiOutInfo>(Info::info))
      if (same(in.name, name))
        return i;
      else
        i++;

    for(const auto& in : get_ports<ValueOutInfo>(Info::info))
      if (same(in.name, name))
        return i;
      else
        i++;

    throw;
  }
  */

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
      std::tuple_size<decltype(get_controls(Info::info))>::value;


  static constexpr auto control_start =
      audio_in_count + midi_in_count + value_in_count;

  static constexpr auto inlet_size =
      audio_in_count + midi_in_count + value_in_count + control_count;

  static constexpr auto outlet_size =
      audio_out_count + midi_out_count + value_out_count;

};

}
