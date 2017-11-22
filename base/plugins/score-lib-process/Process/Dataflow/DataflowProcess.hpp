#pragma once
#include <Process/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <brigand/algorithms.hpp>
#include <type_traits>
#include <ossia/dataflow/graph.hpp>
#include <ossia/network/domain/domain.hpp>
namespace Process {

template<typename T, std::size_t N, typename... Args>
static constexpr auto make(const char (&name)[N], Args&&... args)
{
  return T{QLatin1String(name, N), std::forward<Args>(args)...};
}
struct AudioInInfo {

  QLatin1String name;
  template<std::size_t N>
  constexpr AudioInInfo(const char (&name)[N]): name{name, N} { }
};
struct AudioOutInfo {

  QLatin1String name;
  template<std::size_t N>
  constexpr AudioOutInfo(const char (&name)[N]): name{name, N} { }
};
struct ValueInInfo {

  QLatin1String name;
  template<std::size_t N>
  constexpr ValueInInfo(const char (&name)[N]): name{name, N} { }
};
struct ValueOutInfo {

  QLatin1String name;
  template<std::size_t N>
  constexpr ValueOutInfo(const char (&name)[N]): name{name, N} { }
};
struct MidiInInfo {

  QLatin1String name;
  template<std::size_t N>
  constexpr MidiInInfo(const char (&name)[N]): name{name, N} { }
};
struct MidiOutInfo {

  QLatin1String name;
  template<std::size_t N>
  constexpr MidiOutInfo(const char (&name)[N]): name{name, N} { }
};

enum ControlType: int8_t { Slider };
struct ControlInfo {
  QLatin1String name;
  ControlType type;
  double min;
  double max;

  template<std::size_t N, typename... Args>
  constexpr ControlInfo(const char (&name)[N], ControlType t, double min, double max)://:
    name{name, N}
  , type{t}
  , min{min}
  , max{max}
  {
  }
};

template<typename... Args>
struct NodeInfo: Args...
{
  using types = brigand::list<Args...>;
};

template<typename... Args>
static constexpr auto make_ninfo(Args&&... args)
{
  return NodeInfo<Args...>{std::forward<Args>(args)...};
}
template<typename... Args>
static constexpr auto audio_ins(Args&&... args)
{
  return std::array<AudioInInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
}
template<typename... Args>
static constexpr auto audio_outs(Args&&... args)
{
  return std::array<AudioOutInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
}
template<typename... Args>
static constexpr auto midi_ins(Args&&... args)
{
  return std::array<MidiInInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
}
template<typename... Args>
static constexpr auto midi_outs(Args&&... args)
{
  return std::array<MidiOutInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
}
template<typename... Args>
static constexpr auto value_ins(Args&&... args)
{
  return std::array<ValueInInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
}
template<typename... Args>
static constexpr auto value_outs(Args&&... args)
{
  return std::array<ValueOutInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
}

template<typename... Args>
static constexpr auto control_ins(Args&&... args)
{
  return std::array<ControlInfo, sizeof...(Args)>{{std::forward<Args>(args)...}};
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

template<typename Info>
struct InfoFunctions
{

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

  static constexpr auto inlet_size()
  {
    constexpr auto audio_in = get_ports<AudioInInfo>(Info::info);
    constexpr auto midi_in = get_ports<MidiInInfo>(Info::info);
    constexpr auto value_in = get_ports<ValueInInfo>(Info::info);
    constexpr auto control_in = get_ports<ControlInfo>(Info::info);
    constexpr auto audio_size = audio_in.size();
    constexpr auto midi_size = midi_in.size();
    constexpr auto value_size = value_in.size();
    constexpr auto control_size = control_in.size();
    return audio_size + midi_size + value_size + control_size;
  }

  template<std::size_t N>
  static constexpr auto get_inlet_accessor()
  {
    constexpr auto audio_in = get_ports<AudioInInfo>(Info::info);
    constexpr auto midi_in = get_ports<MidiInInfo>(Info::info);
    constexpr auto value_in = get_ports<ValueInInfo>(Info::info);
    constexpr auto control_in = get_ports<ControlInfo>(Info::info);
    constexpr auto audio_size = audio_in.size();
    constexpr auto midi_size = midi_in.size();
    constexpr auto value_size = value_in.size();
    constexpr auto control_size = control_in.size();
    if constexpr(N < audio_size)
        return [] (const ossia::inlets& inl) { return *inl[N]->data.target<ossia::audio_port>(); };
    else if constexpr(N < (audio_size + midi_size))
        return [] (const ossia::inlets& inl) { return *inl[N]->data.target<ossia::midi_port>(); };
    else if constexpr(N < (audio_size + midi_size + value_size + control_size))
        return [] (const ossia::inlets& inl) { return *inl[N]->data.target<ossia::value_port>(); };
    else
        throw;
  }

  static constexpr auto outlet_size()
  {
    constexpr auto audio_out = get_ports<AudioOutInfo>(Info::info);
    constexpr auto midi_out = get_ports<MidiOutInfo>(Info::info);
    constexpr auto value_out = get_ports<ValueOutInfo>(Info::info);
    constexpr auto audio_size = audio_out.size();
    constexpr auto midi_size = midi_out.size();
    constexpr auto value_size = value_out.size();
    return audio_size + midi_size + value_size;
  }

  template<std::size_t N>
  static constexpr auto get_outlet_accessor()
  {
    constexpr auto audio_out = get_ports<AudioOutInfo>(Info::info);
    constexpr auto midi_out = get_ports<MidiOutInfo>(Info::info);
    constexpr auto value_out = get_ports<ValueOutInfo>(Info::info);
    constexpr auto audio_size = audio_out.size();
    constexpr auto midi_size = midi_out.size();
    constexpr auto value_size = value_out.size();
    if constexpr(N < audio_size)
        return [] (const ossia::outlets& outl) { return *outl[N]->data.target<ossia::audio_port>(); };
    else if constexpr(N < (audio_size + midi_size))
        return [] (const ossia::outlets& outl) { return *outl[N]->data.target<ossia::midi_port>(); };
    else if constexpr(N < (audio_size + midi_size + value_size))
        return [] (const ossia::outlets& outl) { return *outl[N]->data.target<ossia::value_port>(); };
    else
        throw;
  }

  template <class F, std::size_t... I>
  static constexpr decltype(auto) apply_inlet_impl( F&& f, const ossia::inlets& t, std::index_sequence<I...> )
  {
    return f(get_inlet_accessor<I>()(t)...);
  }

  template <class F, std::size_t... I>
  static constexpr decltype(auto) apply_outlet_impl( F&& f, const ossia::outlets& t, std::index_sequence<I...> )
  {
    return f(get_outlet_accessor<I>()(t)...);
  }

  template<typename Fun>
  static void run(
      const ossia::inlets& inlets,
      const ossia::outlets& outlets,
      ossia::token_request tk,
      ossia::execution_state& st,
      Fun f)
  {
    using inlets_indices = std::make_index_sequence<inlet_size()>;
    using outlets_indices = std::make_index_sequence<outlet_size()>;

    // from this, create tuples of functions
    auto vfun = [&] ()
    {
      auto ifun = [&] (auto&&... i)
      {
        return apply_outlet_impl([&] (auto&&... o) { return f(i..., o..., tk, st); }, outlets, outlets_indices{});
      };

      apply_inlet_impl(ifun, inlets, inlets_indices{});
    };

    // apply the functions to inlets and outlets
    vfun();
  }
};

template<typename Info>
class ControlNode : public ossia::graph_node
{
public:
  ControlNode()
  {
    for(const auto& in : get_ports<AudioInInfo>(Info::info))
    {
      m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
    }
    for(const auto& in : get_ports<MidiInInfo>(Info::info))
    {
      m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
    }
    for(const auto& in : get_ports<ValueInInfo>(Info::info))
    {
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    }
    for(const auto& in : get_ports<ControlInfo>(Info::info))
    {
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    }

    for(const auto& out : get_ports<AudioOutInfo>(Info::info))
    {
      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
    }
    for(const auto& out : get_ports<MidiOutInfo>(Info::info))
    {
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
    }
    for(const auto& out : get_ports<ValueOutInfo>(Info::info))
    {
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }
  }

  void run(ossia::token_request tk, ossia::execution_state& st) override
  {
    InfoFunctions<Info>::run(this->inputs(), this->outputs(), tk, st, Info::fun);
  }
};

struct SomeInfo
{
  static const constexpr auto info = make_ninfo(
        audio_ins(AudioInInfo{"audio1"}, AudioInInfo{"audio2"}),
        midi_ins(),
        midi_outs(MidiOutInfo{"midi1"}),
        value_ins(),
        value_outs(),
        control_ins(ControlInfo{"foo", Slider, 1.0, 10.})
        );


  static void fun(
      const ossia::audio_port& p1, const ossia::audio_port& p2, const ossia::value_port& o1,
      ossia::midi_port& p,
      ossia::token_request tk,
      ossia::execution_state& st)
  {
  }
};

auto x = [] {

  ControlNode<SomeInfo> c{};

  ossia::execution_state e;
  c.run({}, e);
};

template<typename Info>
class ControlProcess: public Process::ProcessModel
{
  Process::Inlets m_inlets;
  Process::Outlets m_outlets;

  Process::Inlets inlets() const final override
  {
    return m_inlets;
  }

  Process::Outlets outlets() const final override
  {
    return m_outlets;
  }

  ControlProcess(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    int inlet = 0;
    for(const auto& in : get_ports<AudioInInfo>(Info::info))
    {
      auto p = new Process::Inlet(Id<Process::Port>(inlet++), this);
      p->type = Process::PortType::Audio;
      p->setCustomData(in.name);
      m_inlets.push_back(p);
    }
    for(const auto& in : get_ports<MidiInInfo>(Info::info))
    {
      auto p = new Process::Inlet(Id<Process::Port>(inlet++), this);
      p->type = Process::PortType::Midi;
      p->setCustomData(in.name);
      m_inlets.push_back(p);
    }
    for(const auto& in : get_ports<ValueInInfo>(Info::info))
    {
      auto p = new Process::Inlet(Id<Process::Port>(inlet++), this);
      p->type = Process::PortType::Midi;
      p->setCustomData(in.name);
      m_inlets.push_back(p);
    }
    for(const auto& in : get_ports<ControlInfo>(Info::info))
    {
      auto p = new Process::ControlInlet(Id<Process::Port>(inlet++), this);
      p->type = Process::PortType::Midi;
      p->setDomain(ossia::make_domain(in.min, in.max));
      p->setCustomData(in.name);
      m_inlets.push_back(p);
    }


    int outlet = 0;
    for(const auto& out : get_ports<AudioOutInfo>(Info::info))
    {
      auto p = new Process::Outlet(Id<Process::Port>(outlet++), this);
      p->type = Process::PortType::Audio;
      p->setCustomData(out.name);
      if(outlet == 0)
        p->setPropagate(true);
      m_outlets.push_back(p);
    }
    for(const auto& out : get_ports<MidiOutInfo>(Info::info))
    {
      auto p = new Process::Outlet(Id<Process::Port>(outlet++), this);
      p->type = Process::PortType::Midi;
      p->setCustomData(out.name);
      m_outlets.push_back(p);
    }
    for(const auto& out : get_ports<ValueOutInfo>(Info::info))
    {
      auto p = new Process::Outlet(Id<Process::Port>(outlet++), this);
      p->type = Process::PortType::Midi;
      p->setCustomData(out.name);
      m_outlets.push_back(p);
    }
  }

  ControlProcess(
      const ControlProcess& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent):
    Process::ProcessModel{
      source,
      id,
      Metadata<ObjectKey_k, ProcessModel>::get(),
      parent}

  {
    // Process::clone_outlet(*source.outlet, this);
    metadata().setInstanceName(*this);

  }


  template<typename Impl>
  explicit ControlProcess(
      Impl& vis,
      QObject* parent) :
    Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~ControlProcess() override
  {

  }
};

}
