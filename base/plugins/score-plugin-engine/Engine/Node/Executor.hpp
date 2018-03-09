#pragma once
#include <Engine/Node/Process.hpp>
#include <Engine/Node/TickPolicy.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DeviceList.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/score2OSSIA.hpp>
#include <QTimer>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <readerwriterqueue.h>

namespace Control
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

template<typename Node_T>
class ControlNode final :
    public ossia::graph_node
    , public get_state<Node_T>::type
{
public:
  using info = InfoFunctions<Node_T>;
  static const constexpr bool has_state = has_state_t<Node_T>::value;
  using state_type = typename get_state<Node_T>::type;

  using controls_changed_list = std::bitset<InfoFunctions<Node_T>::control_count>;
  using controls_type = typename InfoFunctions<Node_T>::controls_type;
  using controls_values_type = typename InfoFunctions<Node_T>::controls_values_type;
  controls_values_type controls;
  controls_changed_list controls_changed;
  moodycamel::ReaderWriterQueue<controls_values_type> cqueue;

  template<std::size_t N>
  using timed_vec_t = timed_vec<typename std::tuple_element<N, controls_type>::type::type>;

  template <std::size_t... I>
  static constexpr auto get_control_accessor_types(const std::index_sequence<I...>& )
  {
    return std::tuple<timed_vec_t<I>...>{};
  }

  using control_tuple_t = decltype(get_control_accessor_types(std::make_index_sequence<InfoFunctions<Node_T>::control_count>()));
  control_tuple_t control_tuple;

  ControlNode()
  {
    m_inlets.reserve(InfoFunctions<Node_T>::inlet_size);
    m_outlets.reserve(InfoFunctions<Node_T>::outlet_size);
    for(std::size_t i = 0; i < InfoFunctions<Node_T>::audio_in_count; i++)
    {
      m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Node_T>::midi_in_count; i++)
    {
      m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());
    }
    if constexpr(InfoFunctions<Node_T>::value_in_count > 0)
    {
      constexpr const auto inlet_infos = get_ports<ValueInInfo, Node_T>{}();
      for(std::size_t i = 0; i < InfoFunctions<Node_T>::value_in_count; i++)
      {
        auto inlt = ossia::make_inlet<ossia::value_port>();

        inlt->data.target<ossia::value_port>()->is_event = inlet_infos[i].is_event;
        m_inlets.push_back(std::move(inlt));
      }
    }
    for(std::size_t i = 0; i < InfoFunctions<Node_T>::address_in_count; i++)
    {
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Node_T>::control_count; i++)
    {
      auto inlt = ossia::make_inlet<ossia::value_port>();

      inlt->data.target<ossia::value_port>()->is_event = true;

      m_inlets.push_back(std::move(inlt));
    }

    for(std::size_t i = 0; i < InfoFunctions<Node_T>::audio_out_count; i++)
    {
      m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Node_T>::midi_out_count; i++)
    {
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
    }
    for(std::size_t i = 0; i < InfoFunctions<Node_T>::value_out_count; i++)
    {
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }
  }

  template<std::size_t N>
  static constexpr auto get_inlet_accessor()
  {
    constexpr auto cat = info::categorize_inlet(N);
    if constexpr(cat == inlet_kind::audio_in)
        return [] (const ossia::inlets& inl) -> const ossia::audio_port& { return *inl[N]->data.target<ossia::audio_port>(); };
    else if constexpr(cat == inlet_kind::midi_in)
        return [] (const ossia::inlets& inl) -> const ossia::midi_port& { return *inl[N]->data.target<ossia::midi_port>(); };
    else if constexpr(cat == inlet_kind::value_in)
        return [] (const ossia::inlets& inl) -> const ossia::value_port& { return *inl[N]->data.target<ossia::value_port>(); };
    else if constexpr(cat == inlet_kind::address_in)
        return [] (const ossia::inlets& inl) -> const ossia::destination_t& { return inl[N]->address; };
    else
        throw;
  }


#if defined(_MSC_VER)
#define MSVC_CONSTEXPR const
#else
#define MSVC_CONSTEXPR constexpr
#endif

template<bool Validate, std::size_t N>
struct apply_control;

template<std::size_t N>
struct apply_control<true, N>
{
    template<typename Vec, typename Vp>
    void operator()(Vec& vec, ControlNode& self, const Vp& vp)
    {
        constexpr const auto ctrls = get_controls<Node_T>{}();
        MSVC_CONSTEXPR auto ctrl = std::get<N>(ctrls);
        for (auto& v : vp)
        {
            if (auto res = ctrl.fromValue(v.value))
            {
                vec[int64_t{ v.timestamp }] = *std::move(res);
                self.controls_changed.set(N);
            }
        }
    }
};
template<std::size_t N>
struct apply_control<false, N>
{
    template<typename Vec, typename Vp>
    void operator()(Vec& vec, ControlNode& self, const Vp& vp)
    {
        constexpr const auto ctrls = get_controls<Node_T>{}();
        MSVC_CONSTEXPR auto ctrl = std::get<N>(ctrls);
        for (auto& v : vp)
        {
            vec[int64_t{ v.timestamp }] = ctrl.fromValue(v.value);
            self.controls_changed.set(N);
        }
    }
};
  template<std::size_t N>
  static constexpr auto get_control_accessor()
  {
    return [] (const ossia::inlets& inl, ControlNode& self) -> const auto& {
      constexpr const auto idx = info::control_start + N;
      static_assert(info::control_count > 0);
      static_assert(N < info::control_count);

      using control_type = typename std::tuple_element<N, decltype(get_controls<Node_T>{}())>::type;
      using val_type = typename control_type::type;

      timed_vec<val_type>& vec = std::get<N>(self.control_tuple);
      vec.clear();
      const auto& vp = inl[idx]->data.template target<ossia::value_port>()->get_data();
      vec.reserve(vp.size() + 1);

      // in all cases, set the current value at t=0
      vec.insert(std::make_pair(int64_t{0}, std::get<N>(self.controls)));

      // copy all the values... values arrived later replace previous ones
      apply_control<control_type::must_validate, N>{}(vec, self, vp);

      // the last value will be the first for the next tick
      std::get<N>(self.controls) = vec.rbegin()->second;
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
  
  template<typename T1, typename T3, typename Seq1, typename Seq3>
  struct forwarder;
  
  template<typename T1, typename T3, std::size_t... N1, std::size_t... N3>
  struct forwarder<T1, T3, std::index_sequence<N1...>, std::index_sequence<N3...>>
  {
      T1& a1;
      T3& a3;
      ossia::execution_state& st;
      template<typename... Args>
      void operator()(const ossia::time_value& sub_prev_date,
                      const ossia::token_request& sub_tk,
                      Args&&... args)
      {
        Node_T::run(
              std::get<N1>(std::forward<T1>(a1))...,
              std::forward<Args>(args)...,
              std::get<N3>(std::forward<T3>(a3))...,
              sub_prev_date, sub_tk, st);
        
      }
      
  };
  
  template<typename T1, typename T3, typename State, typename Seq1, typename Seq3>
  struct forwarder_state;
  
  template<typename T1, typename T3, typename State, std::size_t... N1, std::size_t... N3>
  struct forwarder_state<T1, T3, State, std::index_sequence<N1...>, std::index_sequence<N3...>>
  {
      T1& a1;
      T3& a3;
      ossia::execution_state& st;
      State& s;
      template<typename... Args>
      void operator()(const ossia::time_value& sub_prev_date,
                      const ossia::token_request& sub_tk,
                      Args&&... args)
      {
        Node_T::run(
              std::get<N1>(std::forward<T1>(a1))...,
              std::forward<Args>(args)...,
              std::get<N3>(std::forward<T3>(a3))...,
              sub_prev_date, sub_tk, st, s);
        
      }
      
  };
  // Expand three tuples and apply a function on the control tuple
  template<typename F, typename T1, typename T2, typename T3, std::size_t... N1, std::size_t... N2, std::size_t... N3>
  static constexpr auto invoke_impl(F&& f, T1&& a1, T2&& a2, T3&& a3,
                                    const std::index_sequence<N1...>& n1,
                                    const std::index_sequence<N2...>& n2,
                                    const std::index_sequence<N3...>& n3,
                                    const ossia::time_value& prev_date,
                                    const ossia::token_request& tk,
                                    ossia::execution_state& st)
  {
    f(forwarder<T1, T3, std::index_sequence<N1...>, std::index_sequence<N3...>>{a1, a3, st}, prev_date, tk, std::get<N2>(std::forward<T2>(a2))...);
  }
  
  // Expand three tuples and apply a function on the control tuple
  template<typename F, typename T1, typename T2, typename T3, std::size_t... N1, std::size_t... N2, std::size_t... N3, typename State>
  static constexpr auto invoke_impl(F&& f, T1&& a1, T2&& a2, T3&& a3,
                                    const std::index_sequence<N1...>& n1,
                                    const std::index_sequence<N2...>& n2,
                                    const std::index_sequence<N3...>& n3,
                                    const ossia::time_value& prev_date,
                                    const ossia::token_request& tk,
                                    ossia::execution_state& st,
                                    State& s)
  {
    f(forwarder_state<T1, T3, State, std::index_sequence<N1...>, std::index_sequence<N3...>>{a1, a3, st, s}, prev_date, tk, std::get<N2>(std::forward<T2>(a2))...);
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
    using inlets_indices = std::make_index_sequence<info::control_start>;
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
              invoke(typename Node_T::control_policy{}, std::tie(i(inlets)...), std::tie(c(inlets, *this)...), std::tie(o(outlets)...),
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
            Node_T::run(i(inlets)..., o(outlets)...,
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
              invoke(typename Info::control_policy{}, std::tie(i(inlets)...), std::tie(c(inlets, *this)...), std::tie(o(outlets)...), m_prev_date, tk, st);
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

  void all_notes_off() override
  {
    if constexpr(info::midi_in_count > 0)
    {
      // TODO
    }
  }

  std::string label() const override
  {
    return "Control";
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

template<typename T>
struct control_updater
{
    T& control;
    T v;
    void operator()() {
      control = std::move(v);
    }
};


template<typename Info_T, typename Node_T, typename Element>
struct setup_Impl0
{
    Element& element;
    const Engine::Execution::Context& ctx;
    const std::shared_ptr<Node_T>& node_ptr;
    QObject* parent;
    
    template<typename Idx_T>
    struct con_validated
    {
        const Engine::Execution::Context& ctx;
        std::weak_ptr<Node_T> weak_node;
        void operator()(const ossia::value& val)
        {
          constexpr auto idx = Idx_T::value;

          using control_type = typename std::tuple_element<idx, decltype(get_controls<Info_T>{}())>::type;
          using control_value_type = typename control_type::type;

          if (auto node = weak_node.lock())
          {
            constexpr const auto ctrl = std::get<idx>(get_controls<Info_T>{}());
            if (auto v = ctrl.fromValue(val))
              ctx.executionQueue.enqueue(
                    control_updater<control_value_type>{std::get<idx>(node->controls), std::move(*v)});
          }     
        }
    };

    
    template<typename Idx_T>
    struct con_unvalidated
    {
        const Engine::Execution::Context& ctx;
        std::weak_ptr<Node_T> weak_node;
        void operator()(const ossia::value& val)
        {
          constexpr auto idx = Idx_T::value;

          using control_type = typename std::tuple_element<idx, decltype(get_controls<Info_T>{}())>::type;
          using control_value_type = typename control_type::type;

          if (auto node = weak_node.lock())
          {
            constexpr const auto ctrl = std::get<idx>(get_controls<Info_T>{}());
            ctx.executionQueue.enqueue(
                  control_updater<control_value_type>{
                    std::get<idx>(node->controls),
                    ctrl.fromValue(val)});
          }          
        }
    };
    
    template<typename T>
    void operator()(T) 
    {
      using Info = Info_T;
      constexpr int idx = T::value;
      
      constexpr const auto ctrl = std::get<idx>(get_controls<Info_T>{}());
      constexpr const auto control_start = InfoFunctions<Info>::control_start;
      using control_type = typename std::tuple_element<idx, decltype(get_controls<Info_T>{}())>::type;
      using control_value_type = typename control_type::type;
      auto inlet = static_cast<Process::ControlInlet*>(element.inlets()[control_start + idx]);
      
      auto& node = *node_ptr;
      std::weak_ptr<Node_T> weak_node = node_ptr;
      
      if constexpr(control_type::must_validate)
      {
        if (auto res = ctrl.fromValue(element.control(idx)))
          std::get<idx>(node.controls) = *res;
        
        QObject::connect(inlet, &Process::ControlInlet::valueChanged,
                         parent, con_validated<T>{ctx, weak_node});
      }
      else
      {
        std::get<idx>(node.controls) = ctrl.fromValue(element.control(idx));
        
        QObject::connect(inlet, &Process::ControlInlet::valueChanged,
                         parent, con_unvalidated<T>{ctx, weak_node});
      }
      
    }
};

template<typename Info, typename Element, typename Node_T>
struct setup_Impl1
{
    typename Node_T::controls_values_type& arr;
    Element& element;
    
    template<typename T>
    void operator()(T) {
      constexpr const auto ctrl = std::get<T::value>(get_controls<Info>{}());
      
      element.setControl(T::value, ctrl.toValue(std::get<T::value>(arr)));
    }  
};

template<typename Info, typename Node_T, typename Element_T>
void setup_node(const std::shared_ptr<Node_T>& node_ptr
                 , Element_T& element
                 , const Engine::Execution::Context& ctx
                 , QObject* parent)
{
  constexpr const auto control_count = InfoFunctions<Info>::control_count;
  
  if constexpr(control_count > 0)
  {
    // Initialize all the controls in the node with the current value.
    //
    // And update the node when the UI changes
    ossia::for_each_in_range<control_count>(setup_Impl0<Info, Node_T, Element_T>{element, ctx, node_ptr, parent});
    
    // Update the value in the UI
    auto& node = *node_ptr;
    std::weak_ptr<Node_T> weak_node = node_ptr;
    con(ctx.doc.coarseUpdateTimer, &QTimer::timeout,
        parent, [weak_node,&element] {
      if(auto node = weak_node.lock())
      {
        // TODO disconnect the connection ? it will be disconnected shortly after...
        typename Node_T::controls_values_type arr;
        bool ok = false;
        while(node->cqueue.try_dequeue(arr)) {
          ok = true;
        }
        if(ok)
        {
          constexpr const auto control_count = InfoFunctions<Info>::control_count;
          
          ossia::for_each_in_range<control_count>(setup_Impl1<Info, Element_T, Node_T>{arr, element});
          
        }
      }
    }, Qt::QueuedConnection);
  }
}





template<typename Info>
class Executor final: public Engine::Execution::
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
      this->node = node;
      this->m_ossia_process = std::make_shared<ossia::node_process>(this->node);

      setup_node<Info>(node, element, ctx, this);
    }

    ~Executor()
    {
    }
};


}
