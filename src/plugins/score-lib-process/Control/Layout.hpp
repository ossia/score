#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

#include <Process/Dataflow/PortFactory.hpp>

#include <score/graphics/GraphicsLayout.hpp>
namespace Process
{
struct SCORE_LIB_PROCESS_EXPORT LayoutBuilderBase
{
    QObject& context;
    const Process::Context& doc;
    const Process::PortFactoryList& portFactory;

    const Process::Inlets& inlets;
    const Process::Outlets& outlets;

    score::GraphicsLayout* layout{}; // The current container
    std::vector<score::GraphicsLayout*> createdLayouts{};

    QGraphicsItem* makePort(Process::ControlInlet& portModel);
    QGraphicsItem* makePort(Process::ControlOutlet& portModel);
    QGraphicsItem* makeInlet(int index);
    QGraphicsItem* makeOutlet(int index);
    QGraphicsItem* makeLabel(std::string_view item);

    void finalizeLayout(QGraphicsItem* rootItem);


    template<typename T>
    score::BrushSet& get_brush(T cur)
    {
      auto& skin = score::Skin::instance();
      {
      if constexpr(requires { T::darker; })
        if(cur == T::darker)
          return skin.Background2.darker300;
      }
      {
        if constexpr(requires { T::dark; })
          if(cur == T::dark)
            return skin.Background2.darker;
      }
      {
        if constexpr(requires { T::mid; })
          if(cur == T::mid)
            return skin.Background2.main;
      }
      {
        if constexpr(requires { T::light; })
          if(cur == T::light)
            return skin.Background2.lighter;
      }
      {
        if constexpr(requires { T::lighter; })
          if(cur == T::lighter)
            return skin.Background2.lighter180;
      }
      return skin.Background2.main;
    }

    template<typename Item>
    void setupLayout(const Item& it, score::GraphicsLayout& item)
    {
      if constexpr(requires { Item::background(); })
      {
        if constexpr(requires { std::string_view{Item::background()}; })
          item.setBackground(Item::background());
        else
          item.setBrush(get_brush(Item::background()));
      }

      if constexpr(requires { Item::width(); } && requires { Item::height(); })
      {
        item.setRect({0., 0., (qreal)Item::width(), (qreal)Item::height()});
      }
      else if constexpr(requires { Item::width; } && requires { Item::height; })
      {
        item.setRect({0., 0., (qreal)it.width, (qreal)it.height});
      }
    }

    template<typename Item>
    void setupItem(const Item& it, QGraphicsItem& item)
    {
      item.setParentItem(layout);
      if constexpr(requires { Item::x(); } && requires { Item::y(); })
      {
        item.setPos(Item::x(), Item::y());
      }
      else if constexpr(requires { Item::x; } && requires { Item::y; })
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
