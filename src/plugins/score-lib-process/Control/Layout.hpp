#pragma once
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

#include <score/graphics/GraphicsLayout.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/Skin.hpp>

#include <span>

namespace Process
{

struct SCORE_LIB_PROCESS_EXPORT LayoutBuilderBase
{
  QObject& context;
  const Process::ProcessModel& proc;
  const Process::Context& doc;
  const Process::PortFactoryList& portFactory;

  const Process::Inlets& inlets;
  const Process::Outlets& outlets;

  QGraphicsItem* layout{}; // The current container
  std::vector<score::GraphicsLayout*> createdLayouts{};

  Process::ControlLayout makePort(Process::Inlet& portModel);
  Process::ControlLayout makePort(Process::Outlet& portModel);

  std::pair<Process::ControlInlet*, Process::ControlLayout> makeInlet(Process::Inlet*);
  std::pair<Process::ControlOutlet*, Process::ControlLayout>
  makeOutlet(Process::Outlet*);
  std::vector<std::pair<Process::ControlInlet*, Process::ControlLayout>>
      makeInlets(std::span<Process::Inlet*>);
  std::vector<std::pair<Process::ControlOutlet*, Process::ControlLayout>>
      makeOutlets(std::span<Process::Outlet*>);
  QGraphicsItem* makeLabel(std::string_view item);

  void finalizeLayout(QGraphicsItem* rootItem);

  template <typename T>
  score::BrushSet& get_brush(T cur)
  {
    auto& skin = score::Skin::instance();
    {
      if constexpr(requires { T::background_darker; })
        if(cur == T::background_darker)
          return skin.Background2.darker300;
    }
    {
      if constexpr(requires { T::background_dark; })
        if(cur == T::background_dark)
          return skin.Background2.darker;
    }
    {
      if constexpr(requires { T::background_mid; })
        if(cur == T::background_mid)
          return skin.Background2.main;
    }
    {
      if constexpr(requires { T::background_light; })
        if(cur == T::background_light)
          return skin.Background2.lighter;
    }
    {
      if constexpr(requires { T::background_lighter; })
        if(cur == T::background_lighter)
          return skin.Background2.lighter180;
    }
    return skin.Background2.main;
  }

  template <typename Item>
  void setupLayout(const Item& it, score::GraphicsLayout& item)
  {
    if constexpr(requires { Item::background(); })
    {
      if constexpr(requires { std::string_view{Item::background()}; })
        item.setBackground(Item::background());
      else
        item.setBrush(get_brush(Item::background()));
    }

    if constexpr(
        requires { Item::width(); } && requires { Item::height(); })
    {
      item.setRect({0., 0., (qreal)Item::width(), (qreal)Item::height()});
    }
    else if constexpr(
        requires { Item::width; } && requires { Item::height; })
    {
      item.setRect({0., 0., (qreal)it.width, (qreal)it.height});
    }
  }

  template <typename Item>
  void setupItem(const Item& it, QGraphicsItem& item)
  {
    item.setParentItem(layout);
    if constexpr(
        requires { Item::x(); } && requires { Item::y(); })
    {
      item.setPos(Item::x(), Item::y());
    }
    else if constexpr(
        requires { Item::x; } && requires { Item::y; })
    {
      item.setPos(it.x, it.y);
    }

    if constexpr(requires { Item::scale(); })
    {
      item.setScale(Item::scale());
    }
    else if constexpr(requires { Item::scale; })
    {
      item.setScale(it.scale);
    }
  }
};

}
