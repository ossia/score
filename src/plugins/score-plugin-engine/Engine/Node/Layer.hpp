#pragma once
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <Control/Widgets.hpp>
#include <Effect/EffectLayer.hpp>
#include <Engine/Node/Process.hpp>
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

template<typename Info>
struct CustomUISetup
{
  const Process::Inlets& inlets;
  const Process::ProcessModel& process;
  QGraphicsItem& parent;
  QObject& context;
  const score::DocumentContext& doc;

  template <std::size_t N>
  auto& getControl() noexcept
  {
    constexpr int i = ossia::safe_nodes::info_functions<Info>::control_start + N;
    constexpr const auto& ctrl = std::get<N>(Info::Metadata::controls);
    using port_type = typename std::remove_reference_t<decltype(ctrl)>::port_type;
    return static_cast<port_type&>(*inlets[i]);
  }

  template <std::size_t... C>
  void make(const std::index_sequence<C...>&) noexcept
  {
    Info::item(getControl<C>()..., process, parent, context, doc);
  }

  CustomUISetup(
      const Process::Inlets& inlets,
      const Process::ProcessModel& process,
      QGraphicsItem& parent,
      QObject& context,
      const score::DocumentContext& doc)
    : inlets{inlets}, process{process}, parent{parent}, context{context}, doc{doc}
  {
    make(std::make_index_sequence<ossia::safe_nodes::info_functions<Info>::control_count>{});
  }
};

struct AutoUISetup
{
  const Process::Inlets& inlets;
  QGraphicsItem& parent;
  QObject& context;
  const score::DocumentContext& doc;
  const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();

  std::size_t i = 0;
  double pos_y = 0.;

  template <typename Info>
  AutoUISetup(
      Info&&,
      const Process::Inlets& inlets,
      QGraphicsItem& parent,
      QObject& context,
      const score::DocumentContext& doc)
    : inlets{inlets}, parent{parent}, context{context}, doc{doc}
  {
    i = ossia::safe_nodes::info_functions<Info>::control_start;
    ossia::for_each_in_tuple(Info::Metadata::controls, *this);
  }

  // Create a single control
  template<typename T>
  void operator()(const T& ctrl)
  {
    auto item = new score::EmptyItem{&parent};
    item->setPos(0, pos_y);
    auto inlet = static_cast<Process::ControlInlet*>(inlets[i]);

    // Port
    Process::PortFactory* fact = portFactory.get(inlet->concreteKey());
    auto port = fact->makeItem(*inlet, doc, item, &context);

    // Text
    auto lab = new score::SimpleTextItem{score::Skin::instance().Port2.main, item};
    lab->setText(ctrl.name);
    lab->setPos(20., 2.);
    const qreal labelHeight = 10;

    // Control
    QGraphicsItem* widg
        = ctrl.make_item(ctrl, *inlet, doc, nullptr, &context);
    widg->setParentItem(item);
    widg->setPos(18., labelHeight + 5.);

    // Positioning
    port->setPos(8., 4.);

    pos_y += 20.;

    i++;
  }
};

template <typename Info>
class ControlLayerFactory final : public Process::LayerFactory
{
public:
  virtual ~ControlLayerFactory() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& proc,
      QGraphicsItem* parent) const final override
  {
    return new Process::EffectLayerView{parent};
  }

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) const final override
  {
    auto view = safe_cast<Process::EffectLayerView*>(v);
    auto pres = new Process::EffectLayerPresenter{lm, view, context, parent};

    if constexpr (HasCustomUI<Info>::value)
    {
      Control::CustomUISetup<Info>{lm.inlets(), lm, *view, *view, context};
    }
    else if constexpr (ossia::safe_nodes::info_functions<Info>::control_count > 0)
    {
      Control::AutoUISetup{Info{}, lm.inlets(), *view, *view, context};
    }

    return pres;
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent) const final override
  {
    auto rootItem = new score::EmptyRectItem{parent};

    if constexpr (HasCustomUI<Info>::value)
    {
      Control::CustomUISetup<Info>{proc.inlets(), proc, *rootItem, *rootItem, ctx};
    }
    else if constexpr (ossia::safe_nodes::info_functions<Info>::control_count > 0)
    {
      Control::AutoUISetup{Info{}, proc.inlets(), *rootItem, *rootItem, ctx};
    }

    rootItem->setRect(rootItem->childrenBoundingRect());
    return rootItem;
  }
};
}
