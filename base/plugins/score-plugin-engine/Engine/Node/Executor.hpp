#pragma once
#include <Engine/Node/Process.hpp>
#include <Engine/Node/TickPolicy.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/score2OSSIA.hpp>
#include <QTimer>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <readerwriterqueue.h>

namespace Process
{

template<typename T, typename = void>
struct has_event_policy : std::false_type { };
template<typename T>
struct has_event_policy<T, std::void_t<typename T::event_policy>> : std::true_type { };

template<typename T, typename = void>
struct has_audio_policy : std::false_type { };
template<typename T>
struct has_audio_policy<T, std::void_t<typename T::audio_policy>> : std::true_type { };

template<typename T, typename = void>
struct has_control_policy : std::false_type { };
template<typename T>
struct has_control_policy<T, std::void_t<typename T::control_policy>> : std::true_type { };

template<typename Info>
class ControlNode :
    public ossia::graph_node
    , public get_state<Info>::type
{
public:
  using info = InfoFunctions<Info>;
  static const constexpr bool has_state = has_state_t<Info>::value;
  using state_type = typename get_state<Info>::type;

  using controls_list = std::array<ossia::value, InfoFunctions<Info>::control_count>;
  using controls_changed_list = std::bitset<InfoFunctions<Info>::control_count>;
  controls_list controls;
  controls_changed_list controls_changed;
  moodycamel::ReaderWriterQueue<controls_list> cqueue;
  ControlNode()
  {
    m_inlets.reserve(InfoFunctions<Info>::inlet_size);
    m_outlets.reserve(InfoFunctions<Info>::outlet_size);
    for(std::size_t i = 0; i < InfoFunctions<Info>::audio_in_count; i++)
    {
      m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Info>::midi_in_count; i++)
    {
      m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
    }
    if constexpr(InfoFunctions<Info>::value_in_count > 0)
    {
      constexpr const auto inlet_infos = get_ports<ValueInInfo>(Info::info);
      for(std::size_t i = 0; i < InfoFunctions<Info>::value_in_count; i++)
      {
        auto inlt = ossia::make_inlet<ossia::value_port>();
        if(inlet_infos[i].is_event)
          inlt->data.target<ossia::value_port>()->is_event = true;
        m_inlets.push_back(std::move(inlt));
      }
    }
    for(std::size_t i = 0; i < InfoFunctions<Info>::control_count; i++)
    {
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    }

    for(std::size_t i = 0; i < InfoFunctions<Info>::audio_out_count; i++)
    {
      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Info>::midi_out_count; i++)
    {
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Info>::value_out_count; i++)
    {
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }
  }

  template<std::size_t N>
  static constexpr auto get_inlet_accessor()
  {
    if constexpr(N < info::audio_in_count)
        return [] (const ossia::inlets& inl) -> const ossia::audio_port& { return *inl[N]->data.target<ossia::audio_port>(); };
    else if constexpr(N < (info::audio_in_count + info::midi_in_count))
        return [] (const ossia::inlets& inl) -> const ossia::midi_port& { return *inl[N]->data.target<ossia::midi_port>(); };
    else if constexpr(N < (info::audio_in_count + info::midi_in_count + info::value_in_count + info::control_count))
        return [] (const ossia::inlets& inl) -> const ossia::value_port& { return *inl[N]->data.target<ossia::value_port>(); };
    else
        throw;
  }

  template<std::size_t N>
  static constexpr auto get_control_accessor()
  {

    return [] (const ossia::inlets& inl, ControlNode& self) {
      constexpr const auto idx = info::control_start + N;
      static_assert(info::control_count > 0);
      static_assert(N < info::control_count);
    
      using val_type = typename std::tuple_element<N, decltype(get_controls(Info::info))>::type::type;

      // TODO instead, it should go as a member of the node for more perf
      timed_vec<val_type> vec;
      const auto& vp = inl[idx]->data.template target<ossia::value_port>()->get_data();
      vec.reserve(vp.size() + 1);

      // in all cases, set the current value at t=0
      vec.insert(std::make_pair(int64_t{0}, ossia::convert<val_type>(self.controls[N])));

      // copy all the values... values arrived later replace previous ones
      for(auto& v : vp)
      {
        vec[int64_t{v.timestamp}] = ossia::convert<val_type>(v.value);
        self.controls_changed.set(N);
      }

      // the last value will be the first for the next tick
      self.controls[N] = vec.rbegin()->second;
      return vec;
    };
  }

  template<std::size_t N>
  static constexpr auto get_outlet_accessor()
  {
    if constexpr(N < info::audio_out_count)
        return [] (const ossia::outlets& inl) -> ossia::audio_port& { return *inl[N]->data.target<ossia::audio_port>(); };
    else if constexpr(N < (info::audio_out_count + info::midi_out_count))
        return [] (const ossia::outlets& inl) -> ossia::midi_port& { return *inl[N]->data.target<ossia::midi_port>(); };
    else if constexpr(N < (info::audio_out_count + info::midi_out_count + info::value_out_count))
        return [] (const ossia::outlets& inl) -> ossia::value_port& { return *inl[N]->data.target<ossia::value_port>(); };
    else
        throw;
  }

  template <class F, std::size_t... I>
  static constexpr void apply_inlet_impl(const F& f, const std::index_sequence<I...>& )
  {
    f(get_inlet_accessor<I>()...);
  }

  template <class F, std::size_t... I>
  static constexpr void apply_outlet_impl(const F& f, const std::index_sequence<I...>& )
  {
    f(get_outlet_accessor<I>()...);
  }

  template <class F, std::size_t... I>
  static constexpr void apply_control_impl(const F& f, const std::index_sequence<I...>& )
  {
    f(get_control_accessor<I>()...);
  }

  /////////////////

  // Expand three tuples and apply a function on the control tuple
  template<typename F, typename T1, typename T2, typename T3, std::size_t... N1, std::size_t... N2, std::size_t... N3, typename... Args>
  static constexpr auto invoke_impl(F&& f, T1&& a1, T2&& a2, T3&& a3,
                                    std::index_sequence<N1...> n1,
                                    std::index_sequence<N2...> n2,
                                    std::index_sequence<N3...> n3,
                                    const ossia::time_value& prev_date,
                                    const ossia::token_request& tk,
                                    Args&&... rem)
  {
    f([&] (const ossia::time_value& sub_prev_date,
           const ossia::token_request& sub_tk,
           auto&&... args)
    {
      Info::run(
            std::get<N1>(std::forward<T1>(a1))...,
            std::forward<decltype(args)>(args)...,
            std::get<N3>(std::forward<T3>(a3))...,
            sub_prev_date, sub_tk, std::forward<Args>(rem)...);
    }, prev_date, tk, std::get<N2>(std::forward<T2>(a2))...);
  }

  template<typename F, typename T1, typename T2, typename T3, typename... Args>
  static constexpr auto invoke(
      F&& f,
        T1&& a1, T2&& a2, T3&& a3,
        Args&&... args)
  {
    using I1 = std::make_index_sequence<std::tuple_size_v<std::decay_t<T1>>>;
    using I2 = std::make_index_sequence<std::tuple_size_v<std::decay_t<T2>>>;
    using I3 = std::make_index_sequence<std::tuple_size_v<std::decay_t<T3>>>;

    return invoke_impl(f, std::forward<T1>(a1), std::forward<T2>(a2), std::forward<T3>(a3),
                       I1{}, I2{}, I3{}, std::forward<Args>(args)...);
  }

  void run(ossia::token_request tk, ossia::execution_state& st) override
  {
#if !defined(_MSC_VER)
    using inlets_indices = std::make_index_sequence<info::audio_in_count + info::midi_in_count + info::value_in_count>;
    using controls_indices = std::make_index_sequence<info::control_count>;
    using outlets_indices = std::make_index_sequence<info::outlet_size>;

    ossia::inlets& inlets = this->inputs();
    ossia::outlets& outlets = this->outputs();

    if constexpr(has_state)
    {
      if constexpr(info::control_count > 0) {
        // from this, create tuples of functions
        // apply the functions to inlets and outlets
        apply_inlet_impl(
              [&] (auto&&... i) {
          apply_control_impl(
                [&] (auto&&... c) {
            apply_outlet_impl(
                  [&] (auto&&... o) {
              invoke(typename Info::control_policy{}, std::tie(i(inlets)...), std::make_tuple(c(inlets, *this)...), std::tie(o(outlets)...),
                       m_prev_date, tk, st, static_cast<state_type&>(*this));
            }, outlets_indices{});
          }, controls_indices{});
        }, inlets_indices{});
      }
      else
      {
        apply_inlet_impl(
              [&] (auto&&... i) {
          apply_outlet_impl(
                [&] (auto&&... o) {
            Info::run(i(inlets)..., o(outlets)...,
                      m_prev_date, tk, st, static_cast<state_type&>(*this));
          }, outlets_indices{});
        }, inlets_indices{});
      }
    }
    else
    {
      if constexpr(info::control_count > 0) {
        // from this, create tuples of functions
        // apply the functions to inlets and outlets
        apply_inlet_impl(
              [&] (auto&&... i) {
          apply_control_impl(
                [&] (auto&&... c) {
            apply_outlet_impl(
                  [&] (auto&&... o) {
              invoke(typename Info::control_policy{}, std::tie(i(inlets)...), std::make_tuple(c(inlets, *this)...), std::tie(o(outlets)...), m_prev_date, tk, st);
            }, outlets_indices{});
          }, controls_indices{});
        }, inlets_indices{});
      }
      else
      {
        apply_inlet_impl(
              [&] (auto&&... i) {
          apply_outlet_impl(
                [&] (auto&&... o) {
            Info::run(i(inlets)..., o(outlets)..., m_prev_date, tk, st);
          }, outlets_indices{});
        }, inlets_indices{});
      }
    }

    if(cqueue.size_approx() < 1 && controls_changed.any())
    {
      cqueue.try_enqueue(controls);
      controls_changed.reset();
    }
#endif
  }
};

struct value_adder
{
    ossia::value_port& port;
    ossia::value v;
    void operator()() {
      // timestamp should be > all others so that it is always active ?
      port.add_raw_value(std::move(v));
    }
};

struct control_updater
{
    ossia::value& control;
    ossia::value v;
    void operator()() {
      control = std::move(v);
    }
};

template<typename Info>
class Executor: public Engine::Execution::
    ProcessComponent_T<ControlProcess<Info>, ossia::node_process>
{
  public:
    static Q_DECL_RELAXED_CONSTEXPR score::Component::Key static_key()
    {
      return Info::Metadata::uuid;
    }

    score::Component::Key key() const final override
    {
      return static_key();
    }

    bool key_match(score::Component::Key other) const final override
    {
      return static_key() == other
             || Engine::Execution::ProcessComponent::base_key_match(other);
    }

    Executor(
        ControlProcess<Info>& element,
        const ::Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent):
      Engine::Execution::ProcessComponent_T<ControlProcess<Info>, ossia::node_process>{
                            element,
                            ctx,
                            id, "Executor::ControlProcess<Info>", parent}
    {
      auto node = std::make_shared<ControlNode<Info>>();
      auto proc = std::make_shared<ossia::node_process>(node);
      this->m_node = node;
      this->m_ossia_process = proc;
      const auto& dl = ctx.devices.list();

      constexpr const auto control_start = InfoFunctions<Info>::control_start;
      constexpr const auto control_count = InfoFunctions<Info>::control_count;


      for(std::size_t i = 0; i < InfoFunctions<Info>::inlet_size; i++)
      {
        auto dest = Engine::score_to_ossia::makeDestination(dl, element.inlets_ref()[i]->address());
        if(dest)
        {
          node->inputs()[i]->address = &dest->address();
        }
      }
      if constexpr(control_count > 0)
      {
        for(std::size_t i = 0; i < control_count; i++)
        {
          node->controls[i] = element.control(i);
        }

        con(ctx.doc.coarseUpdateTimer, &QTimer::timeout,
            this, [&,node] {
          typename ControlNode<Info>::controls_list arr;
          bool ok = false;
          while(node->cqueue.try_dequeue(arr)) {
            ok = true;
          }
          if(ok)
          {
            for(std::size_t i = 0; i < control_count; i++)
            {
              element.setControl(i, arr[i]);
            }
          }
        });
      }


      for(std::size_t i = 0; i < InfoFunctions<Info>::outlet_size; i++)
      {
        auto dest = Engine::score_to_ossia::makeDestination(dl, element.outlets_ref()[i]->address());
        if(dest)
        {
          node->outputs()[i]->address = &dest->address();
        }
      }

      for(std::size_t idx = control_start; idx < control_start + control_count; idx++)
      {
        auto inlet = static_cast<ControlInlet*>(element.inlets_ref()[idx]);
        //auto port = node->inputs()[idx]->data.template target<ossia::value_port>();

        QObject::connect(inlet, &ControlInlet::valueChanged,
                this, [=] (const ossia::value& val) {
          //this->system().executionQueue.enqueue(value_adder{*port, val});
          this->system().executionQueue.enqueue(control_updater{node->controls[idx - control_start], val});

        });
      }

      ctx.plugin.register_node(element, node);
    }


    ~Executor()
    {
      this->system().plugin.unregister_node(this->process(), m_node);
    }

  private:
    ossia::node_ptr m_node;
};


}
