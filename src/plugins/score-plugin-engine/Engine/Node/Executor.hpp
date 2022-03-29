#pragma once
#include <Engine/Node/Process.hpp>
#include <Engine/Node/TickPolicy.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/safe_nodes/executor.hpp>

#include <QTimer>

namespace Control
{

template <typename Info_T, typename Node_T, typename Element>
struct setup_Impl0
{
  Element& element;
  const Execution::Context& ctx;
  const std::shared_ptr<Node_T>& node_ptr;
  QObject* parent;

  template <typename Idx_T>
  struct con_validated
  {
    const Execution::Context& ctx;
    std::weak_ptr<Node_T> weak_node;
    void operator()(const ossia::value& val)
    {
      using namespace ossia::safe_nodes;
      constexpr auto idx = Idx_T::value;

      using control_type = typename std::
          tuple_element<idx, decltype(Info_T::Metadata::controls)>::type;
      using control_value_type = typename control_type::type;

      if (auto node = weak_node.lock())
      {
        constexpr const auto ctrl = tuplet::get<idx>(Info_T::Metadata::controls);
        if (auto v = ctrl.fromValue(val))
          ctx.executionQueue.enqueue(control_updater<control_value_type>{
              tuplet::get<idx>(node->controls), std::move(*v)});
      }
    }
  };

  template <typename Idx_T>
  struct con_unvalidated
  {
    const Execution::Context& ctx;
    std::weak_ptr<Node_T> weak_node;
    void operator()(const ossia::value& val)
    {
      using namespace ossia::safe_nodes;
      constexpr auto idx = Idx_T::value;

      using control_type = typename std::
          tuple_element<idx, decltype(Info_T::Metadata::controls)>::type;
      using control_value_type = typename control_type::type;

      if (auto node = weak_node.lock())
      {
        constexpr const auto ctrl = tuplet::get<idx>(Info_T::Metadata::controls);
        ctx.executionQueue.enqueue(control_updater<control_value_type>{
            tuplet::get<idx>(node->controls), ctrl.fromValue(val)});
      }
    }
  };

  template <typename T>
  void operator()(T)
  {
    using namespace ossia::safe_nodes;
    using Info = Info_T;
    constexpr int idx = T::value;

    constexpr const auto ctrl = tuplet::get<idx>(Info_T::Metadata::controls);
    constexpr const auto control_start = info_functions<Info>::control_start;
    using control_type = typename std::
        tuple_element<idx, decltype(Info_T::Metadata::controls)>::type;
    auto inlet = static_cast<Process::ControlInlet*>(
        element.inlets()[control_start + idx]);

    auto& node = *node_ptr;
    std::weak_ptr<Node_T> weak_node = node_ptr;

    if constexpr (control_type::must_validate)
    {
      if (auto res = ctrl.fromValue(element.control(idx)))
        tuplet::get<idx>(node.controls) = *res;

      QObject::connect(
          inlet,
          &Process::ControlInlet::valueChanged,
          parent,
          con_validated<T>{ctx, weak_node});
    }
    else
    {
      tuplet::get<idx>(node.controls) = ctrl.fromValue(element.control(idx));

      QObject::connect(
          inlet,
          &Process::ControlInlet::valueChanged,
          parent,
          con_unvalidated<T>{ctx, weak_node});
    }
  }
};

template <typename Info, typename Element, typename Node_T>
struct setup_Impl1
{
  typename Node_T::controls_values_type& arr;
  Element& element;

  template <typename T>
  void operator()(T)
  {
    using namespace ossia::safe_nodes;
    using namespace std;
    using namespace tuplet;
    constexpr const auto ctrl = tuplet::get<T::value>(Info::Metadata::controls);

    element.setControl(T::value, ctrl.toValue(get<T::value>(arr)));
  }
};

template <typename Info, typename Element, typename Node_T>
struct setup_Impl1_Out
{
  typename Node_T::control_outs_values_type& arr;
  Element& element;

  template <typename T>
  void operator()(T)
  {
    using namespace ossia::safe_nodes;
    using namespace std;
    using namespace tuplet;
    constexpr const auto ctrl
        = tuplet::get<T::value>(Info::Metadata::control_outs);

    element.setControlOut(T::value, ctrl.toValue(get<T::value>(arr)));
  }
};

template <typename Info, typename Node_T, typename Element_T>
struct ExecutorGuiUpdate
{
  std::weak_ptr<Node_T> weak_node;
  Element_T& element;

  void handle_controls(Node_T& node) const noexcept
  {
    using namespace ossia::safe_nodes;
    // TODO disconnect the connection ? it will be disconnected shortly
    // after...
    typename Node_T::controls_values_type arr;
    bool ok = false;
    while (node.cqueue.try_dequeue(arr))
    {
      ok = true;
    }
    if (ok)
    {
      constexpr const auto control_count = info_functions<Info>::control_count;

      ossia::for_each_in_range<control_count>(
          setup_Impl1<Info, Element_T, Node_T>{arr, element});
    }
  }

  void handle_control_outs(Node_T& node) const noexcept
  {
    using namespace ossia::safe_nodes;
    // TODO disconnect the connection ? it will be disconnected shortly
    // after...
    typename Node_T::control_outs_values_type arr;
    bool ok = false;
    while (node.control_outs_queue.try_dequeue(arr))
    {
      ok = true;
    }
    if (ok)
    {
      constexpr const auto control_out_count
          = info_functions<Info>::control_out_count;

      ossia::for_each_in_range<control_out_count>(
          setup_Impl1_Out<Info, Element_T, Node_T>{arr, element});
    }
  }

  void operator()() const noexcept
  {
    using namespace ossia::safe_nodes;
    if (auto node = weak_node.lock())
    {
      if constexpr (info_functions<Info>::control_count > 0)
        handle_controls(*node);

      if constexpr (info_functions<Info>::control_out_count > 0)
        handle_control_outs(*node);
    }
  }
};

template <typename Info, typename Node_T, typename Element_T>
void setup_node(
    const std::shared_ptr<Node_T>& node_ptr,
    Element_T& element,
    const Execution::Context& ctx,
    QObject* parent)
{
  using namespace ossia::safe_nodes;

  (void)parent;
  if constexpr (info_functions<Info>::control_count > 0)
  {
    // Initialize all the controls in the node with the current value.
    //
    // And update the node when the UI changes
    ossia::for_each_in_range<info_functions<Info>::control_count>(
        setup_Impl0<Info, Node_T, Element_T>{element, ctx, node_ptr, parent});
  }

  if constexpr (
      info_functions<Info>::control_count > 0
      || info_functions<Info>::control_out_count > 0)
  {
    // Update the value in the UI
    std::weak_ptr<Node_T> weak_node = node_ptr;
    con(ctx.doc.coarseUpdateTimer,
        &QTimer::timeout,
        parent,
        ExecutorGuiUpdate<Info, Node_T, Element_T>{weak_node, element},
        Qt::QueuedConnection);
  }
}

template <typename Info>
class Executor final
    : public Execution::
          ProcessComponent_T<ControlProcess<Info>, ossia::node_process>
{
public:
  static Q_DECL_RELAXED_CONSTEXPR UuidKey<score::Component>
  static_key() noexcept
  {
    return Info::Metadata::uuid;
  }

  UuidKey<score::Component> key() const noexcept final override
  {
    return static_key();
  }

  bool key_match(UuidKey<score::Component> other) const noexcept final override
  {
    return static_key() == other
           || Execution::ProcessComponent::base_key_match(other);
  }

  Executor(
      ControlProcess<Info>& element,
      const ::Execution::Context& ctx,
      QObject* parent)
      : Execution::
          ProcessComponent_T<ControlProcess<Info>, ossia::node_process>{
              element,
              ctx,
              "Executor::ControlProcess<Info>",
              parent}
  {
    std::shared_ptr< ossia::safe_nodes::safe_node<Info> > n{new ossia::safe_nodes::safe_node<Info>};
    n->prepare(*ctx.execState.get());
    this->node = n;
    this->m_ossia_process = std::make_shared<ossia::node_process>(this->node);

    setup_node<Info>(n, element, ctx, this);
  }

  ~Executor() { }
};
}
