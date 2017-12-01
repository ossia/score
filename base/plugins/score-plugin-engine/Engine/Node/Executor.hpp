#pragma once
#include <Engine/Node/Process.hpp>
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
struct has_run : std::false_type { };
template<typename T>
struct has_run<T, std::void_t<decltype(&T::run)>> : std::true_type { };

template<typename T, typename = void>
struct has_run_precise : std::false_type { };
template<typename T>
struct has_run_precise<T, std::void_t<decltype(&T::run_precise)>> : std::true_type { };

template<typename T, typename = void>
struct has_run_last: std::false_type { };
template<typename T>
struct has_run_last<T, std::void_t<decltype(&T::run_last)>> : std::true_type { };

template<typename T, typename = void>
struct has_run_first_last: std::false_type { };
template<typename T>
struct has_run_first_last<T, std::void_t<decltype(&T::run_first_last)>> : std::true_type { };


template<typename... Args>
auto timestamp(const std::pair<Args...>& p)
{
  return p.first;
}
template<typename T>
auto timestamp(const T& p)
{
  return p.timestamp;
}


template<typename Info>
class ControlNode :
    public ossia::graph_node
    , public decltype(get_state(Info::info))
{
public:
  using info = InfoFunctions<Info>;
  using state_type = decltype(get_state(Info::info));
  static const constexpr bool has_state = !std::is_same_v<state_type, dummy_t>;

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
    constexpr const auto inlet_infos = get_ports<ValueInInfo>(Info::info);
    if constexpr(InfoFunctions<Info>::value_in_count > 0)
    {
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
    constexpr const auto idx = info::control_start + N;
    static_assert(info::control_count > 0);
    static_assert(N < info::control_count);

    return [] (const ossia::inlets& inl, ControlNode& self) {
      constexpr const auto controls = get_controls(Info::info);
      constexpr const auto control = std::get<N>(controls);
      using val_type = typename decltype(control)::type;

      // TODO instead, it should go as a member of the node for more perf
      timed_vec<val_type> vec;
      const auto& vp = inl[idx]->data.template target<ossia::value_port>()->get_data();
      vec.reserve(vp.size() + 1);

      // in all cases, set the current value at t=0
      vec.insert(std::make_pair(int64_t{0}, ossia::convert<val_type>(self.controls[N])));

      // copy all the values
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
  template<typename TickFun, typename... Args>
  static auto precise_sub_tick(TickFun f, ossia::time_value prev_date, ossia::token_request req, const Process::timed_vec<Args>&... arg)
  {
    constexpr const std::size_t N = sizeof...(arg);
    auto iterators = std::make_tuple(arg.begin()...);
    const auto last_iterators = std::make_tuple(--arg.end()...);

    // while all the it are != arg.rbegin(),
    //  increment the smallest one
    //  call a tick with this at the new date

    auto reached_end = [&] {
      bool b = true;
      ossia::for_each_in_range<N>([&b,&iterators,&last_iterators] (auto i) {
        b &= (std::get<i.value>(iterators) == std::get<i.value>(last_iterators));
      });
      return b;
    };

    //const auto parent_dur = req.date / req.position;
    auto call_f = [&] (ossia::time_value cur) {
      ossia::token_request r = req;
      //r.date +=
      std::apply([&] (const auto&... it) { f(prev_date, r, it->second...); }, iterators);
    };

    ossia::time_value current_time = req.offset;
    while(!reached_end())
    {
      // run a tick with the current values (TODO pass the current time too)
      call_f(current_time);

      std::bitset<sizeof...(Args)> to_increment;
      to_increment.reset();
      auto min = ossia::Infinite;
      ossia::for_each_in_range<N>([&] (auto idx_t) {
        constexpr auto idx = idx_t.value;
        auto& it = std::get<idx>(iterators);
        if(it != std::get<idx>(last_iterators))
        {
          auto next = it; ++next;
          const auto next_ts = timestamp(*next);
          const auto diff = next_ts - current_time;
          if(diff < 0)
          {
            // token before offset, we increment in all cases
            it = next;
            return;
          }

          if(diff < min)
          {
            min = diff;
            to_increment.reset();
            to_increment.set(idx);
          }
          else if(diff == min)
          {
            to_increment.set(idx);
          }
        }
      });

      current_time += min;
      ossia::for_each_in_range<N>([&] (auto idx_t)
      {
        constexpr auto idx = idx_t.value;
        if(to_increment.test(idx))
        {
          ++std::get<idx>(iterators);
        }
      });
    }

    call_f(current_time);
  }

  template<typename TickFun, typename... Args>
  static auto last_sub_tick(TickFun f, ossia::time_value prev_date, ossia::token_request req, const Process::timed_vec<Args>&... arg)
  {
    // TODO use largest date instead
    std::apply([&] (const auto&... it) { f(prev_date, req, it->second...); }, std::make_tuple(--arg.end()...));
  }
  template<typename TickFun, typename... Args>
  static auto first_last_sub_tick(TickFun f, ossia::time_value prev_date, ossia::token_request req, const Process::timed_vec<Args>&... arg)
  {
    // TODO use correct dates
    std::apply([&] (const auto&... it) { f(prev_date, req, it->second...); }, std::make_tuple(arg.begin()...));
    std::apply([&] (const auto&... it) { f(prev_date, req, it->second...); }, std::make_tuple(--arg.end()...));
  }

  template<typename T, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run_precise(
        T1&& a1, T2&& a2, T3&& a3,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        precise_sub_tick([&] (ossia::time_value prev_date, ossia::token_request req, auto&&... args) {
          std::apply([&] (auto&&... o) {
            T::run_precise(in..., args..., o..., prev_date, req, st);
          }, std::forward<T3>(a3));
        }, prev_date, tk, c...);
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }
  template<typename T, typename S, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run_precise(
        T1&& a1, T2&& a2, T3&& a3,
        S& s,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        precise_sub_tick([&] (ossia::time_value prev_date, ossia::token_request req, auto&&... args) {
          std::apply([&] (auto&&... o) {
            T::run_precise(in..., args..., o..., s, prev_date, req, st);
          }, std::forward<T3>(a3));
        }, prev_date, tk, c...);
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }

  template<typename T, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run_last(
        T1&& a1, T2&& a2, T3&& a3,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        last_sub_tick([&] (ossia::time_value prev_date, ossia::token_request req, auto&&... args) {
          std::apply([&] (auto&&... o) {
            T::run_last(in..., args..., o..., prev_date, req, st);
          }, std::forward<T3>(a3));
        }, prev_date, tk, c...);
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }
  template<typename T, typename S, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run_last(
        T1&& a1, T2&& a2, T3&& a3,
        S& s,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        last_sub_tick([&] (ossia::time_value prev_date, ossia::token_request req, auto&&... args) {
          std::apply([&] (auto&&... o) {
            T::run_last(in..., args..., o..., s, prev_date, req, st);
          }, std::forward<T3>(a3));
        }, prev_date, tk, c...);
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }


  template<typename T, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run_first_last(
        T1&& a1, T2&& a2, T3&& a3,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        first_last_sub_tick([&] (ossia::time_value prev_date, ossia::token_request req, auto&&... args) {
          std::apply([&] (auto&&... o) {
            T::run_first_last(in..., args..., o..., prev_date, req, st);
          }, std::forward<T3>(a3));
        }, prev_date, tk, c...);
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }
  template<typename T, typename S, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run_first_last(
        T1&& a1, T2&& a2, T3&& a3,
        S& s,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        first_last_sub_tick([&] (ossia::time_value prev_date, ossia::token_request req, auto&&... args) {
          std::apply([&] (auto&&... o) {
            T::run_first_last(in..., args..., o..., s, prev_date, req, st);
          }, std::forward<T3>(a3));
        }, prev_date, tk, c...);
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }

  template<typename T, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run(
        T1&& a1, T2&& a2, T3&& a3,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        std::apply([&] (auto&&... o) {
          T::run(in..., c..., o..., prev_date, tk, st);
        }, std::forward<T3>(a3));
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }
  template<typename T, typename S, typename T1, typename T2, typename T3>
  static constexpr auto wrap_run(
        T1&& a1, T2&& a2, T3&& a3,
        S& s,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
  {
    std::apply([&] (auto&&... in) {
      std::apply([&] (auto&&... c) {
        std::apply([&] (auto&&... o) {
          T::run(in..., c..., o..., s, prev_date, tk, st);
        }, std::forward<T3>(a3));
      }, std::forward<T2>(a2));
    }, std::forward<T1>(a1));
  }

  template<typename T, typename... Args>
  static constexpr auto run_function(Args&&... args)
  {
    static_assert(has_run_precise<T>::value || has_run_last<T>::value || has_run_first_last<T>::value || has_run<T>::value);
    if constexpr(has_run_precise<T>::value) return wrap_run_precise<T>(std::forward<Args>(args)...);
    else if constexpr(has_run_last<T>::value) return wrap_run_last<T>(std::forward<Args>(args)...);
    else if constexpr(has_run_first_last<T>::value) return wrap_run_first_last<T>(std::forward<Args>(args)...);
    else if constexpr(has_run<T>::value) return wrap_run<T>(std::forward<Args>(args)...);
    else throw;
  }




  void run(ossia::token_request tk, ossia::execution_state& st) override
  {
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
              run_function<Info>(std::tie(i(inlets)...), std::make_tuple(c(inlets, *this)...), std::tie(o(outlets)...),
                       static_cast<state_type&>(*this), m_prev_date, tk, st);
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
            Info::run(i(inlets)..., o(outlets)..., static_cast<state_type&>(*this),
                     m_prev_date, tk, st);
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
              run_function<Info>(std::tie(i(inlets)...), std::make_tuple(c(inlets, *this)...), std::tie(o(outlets)...), m_prev_date, tk, st);
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
  }
};

struct value_adder
{
    ossia::value_port& port;
    ossia::value v;
    void operator()() {
      // timestamp should be > all others so that it is always active ?
      port.add_value(std::move(v));
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
