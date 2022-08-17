#pragma once

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Control/Widgets.hpp>
#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
#include <Engine/Node/Process.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

#include <tuplet/tuple.hpp>

#include <type_traits>

namespace Control
{
template <typename T, typename = void>
struct HasCustomUI : std::false_type
{
};
template <typename T>
struct HasCustomUI<T, std::void_t<decltype(&T::item)>> : std::true_type
{
};
template <typename T, typename = void>
struct HasCustomLayer : std::false_type
{
};
template <typename T>
struct HasCustomLayer<T, std::void_t<typename T::Layer>> : std::true_type
{
};

template <typename Info>
struct CustomUISetup
{
  const Process::Inlets& inlets;
  const Process::Outlets& outlets;
  const Process::ProcessModel& process;
  QGraphicsItem& parent;
  QObject& context;
  const Process::Context& doc;

  template <std::size_t N>
  auto& getControl() noexcept
  {
    using namespace std;
    using namespace tuplet;

    constexpr int i = ossia::safe_nodes::info_functions<Info>::control_start + N;
    constexpr const auto& ctrl = get<N>(Info::Metadata::controls);
    using port_type = decltype(*ctrl.create_inlet(Id<Process::Port>{}, nullptr));
    return static_cast<port_type&>(*inlets[i]);
  }

  template <std::size_t N>
  auto& getControlOut() noexcept
  {
    using namespace std;
    using namespace tuplet;

    constexpr int i = ossia::safe_nodes::info_functions<Info>::control_out_start + N;
    constexpr const auto& ctrl = get<N>(Info::Metadata::control_outs);
    using port_type = decltype(*ctrl.create_outlet(Id<Process::Port>{}, nullptr));
    return static_cast<port_type&>(*outlets[i]);
  }

  template <std::size_t... CI, std::size_t... CO>
  void make(std::index_sequence<CI...>, std::index_sequence<CO...>) noexcept
  {
    Info::item(
        getControl<CI>()..., getControlOut<CO>()..., process, parent, context, doc);
  }

  CustomUISetup(
      const Process::Inlets& inlets, const Process::Outlets& outlets,
      const Process::ProcessModel& process, QGraphicsItem& parent, QObject& context,
      const Process::Context& doc)
      : inlets{inlets}
      , outlets{outlets}
      , process{process}
      , parent{parent}
      , context{context}
      , doc{doc}
  {
    make(
        std::make_index_sequence<
            ossia::safe_nodes::info_functions<Info>::control_count>{},
        std::make_index_sequence<
            ossia::safe_nodes::info_functions<Info>::control_out_count>{});
  }
};

template <typename Info>
requires(
    HasCustomLayer<Info>::value
    || HasCustomUI<Info>::value) class ControlLayerFactory final
    : public Process::LayerFactory
{
public:
  virtual ~ControlLayerFactory() = default;

private:
  std::optional<double> recommendedHeight() const noexcept override
  {
    if constexpr(Info::Metadata::recommended_height > 0)
    {
      return Info::Metadata::recommended_height;
    }
    return LayerFactory::recommendedHeight();
  }

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& proc, const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    if constexpr(HasCustomLayer<Info>::value)
      return new typename Info::Layer{proc, context, parent};
    else
      return new Process::EffectLayerView{parent};
  }

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm, Process::LayerView* v,
      const Process::Context& context, QObject* parent) const final override
  {
    if constexpr(HasCustomLayer<Info>::value)
    {
      auto view = static_cast<typename Info::Layer*>(v);
      return new Process::EffectLayerPresenter{lm, view, context, parent};
    }
    else if constexpr(HasCustomUI<Info>::value)
    {
      auto view = safe_cast<Process::EffectLayerView*>(v);
      auto pres = new Process::EffectLayerPresenter{lm, view, context, parent};

      Control::CustomUISetup<Info>{lm.inlets(), lm.outlets(), lm, *view, *view, context};
      return pres;
    }
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc, const Process::Context& ctx,
      QGraphicsItem* parent) const final override
  {
    if constexpr(HasCustomLayer<Info>::value)
    {
      // We want to go through the makeLayerPresenter case here
      return nullptr;
    }
    else if constexpr(HasCustomUI<Info>::value)
    {
      auto rootItem = new score::EmptyRectItem{parent};
      Control::CustomUISetup<Info>{proc.inlets(), proc.outlets(), proc,
                                   *rootItem,     *rootItem,      ctx};

      rootItem->fitChildrenRect();

      QObject::connect(
          rootItem, &score::ResizeableItem::childrenSizeChanged, rootItem,
          &score::EmptyRectItem::fitChildrenRect);
      return rootItem;
    }
  }
};
}
