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

  //! The skin brush a layout colour names. An object only ever asks for a role
  //! - a background, the value it edits, the value the engine sends back - so
  //! that a skin change applies to it like it does to the rest of the software.
  template <typename T>
  score::BrushSet& get_brush(T cur)
  {
    auto& skin = score::Skin::instance();
#define SCORE_MAP_LAYOUT_COLOR(Name, Brush)  \
  if constexpr(requires { T::Name; })        \
    if(cur == T::Name)                       \
      return Brush;

    SCORE_MAP_LAYOUT_COLOR(darker, skin.Gray.darker300)
    SCORE_MAP_LAYOUT_COLOR(dark, skin.Gray.darker)
    SCORE_MAP_LAYOUT_COLOR(mid, skin.Gray.main)
    SCORE_MAP_LAYOUT_COLOR(light, skin.Gray.lighter)
    SCORE_MAP_LAYOUT_COLOR(lighter, skin.Gray.lighter180)

    SCORE_MAP_LAYOUT_COLOR(background_darker, skin.Background2.darker300)
    SCORE_MAP_LAYOUT_COLOR(background_dark, skin.Background2.darker)
    SCORE_MAP_LAYOUT_COLOR(background_mid, skin.Background2.main)
    SCORE_MAP_LAYOUT_COLOR(background_light, skin.Background2.lighter)
    SCORE_MAP_LAYOUT_COLOR(background_lighter, skin.Background2.lighter180)

    // Base4 is what a slider fills its handle with, Base1 what it draws the
    // value the execution sends back with.
    SCORE_MAP_LAYOUT_COLOR(editable_value_dark, skin.Base4.darker)
    SCORE_MAP_LAYOUT_COLOR(editable_value_mid, skin.Base4.main)
    SCORE_MAP_LAYOUT_COLOR(editable_value_light, skin.Base4.lighter180)

    SCORE_MAP_LAYOUT_COLOR(runtime_value_dark, skin.Base1.darker)
    SCORE_MAP_LAYOUT_COLOR(runtime_value_mid, skin.Base1.main)
    SCORE_MAP_LAYOUT_COLOR(runtime_value_light, skin.Base1.lighter180)
#undef SCORE_MAP_LAYOUT_COLOR

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
