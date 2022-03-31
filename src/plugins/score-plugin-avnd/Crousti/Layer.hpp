#pragma once
#include <Crousti/ProcessModel.hpp>
#include <Crousti/Painter.hpp>
#include <Control/Layout.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <avnd/concepts/layout.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <score/graphics/layouts/GraphicsSplitLayout.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>


namespace oscr
{

template<typename Info>
struct LayoutBuilder final : Process::LayoutBuilderBase
{
    using inputs_type = typename avnd::input_introspection<Info>::type;
    using outputs_type = typename avnd::output_introspection<Info>::type;

    template<typename T>
    QGraphicsItem* createControl(auto T::*member)
    {
      if constexpr(requires { ((inputs_type{}).*member); })
      {
        const int index = avnd::index_in_struct(inputs_type{}, member);
        return makeInlet(index);
      }
      else
      {
        if constexpr(requires { ((outputs_type{}).*member); })
        {
          const int index = avnd::index_in_struct(outputs_type{}, member);
          return makeOutlet(index);
        }
      }

      return nullptr;
    }

    template<typename T>
    QGraphicsItem* createWidget(const T& item)
    {
      if constexpr(requires { { item } -> std::convertible_to<std::string_view>; })
      {
        return makeLabel(item);
      }
      else if constexpr(requires { { item.text } -> std::convertible_to<std::string_view>; })
      {
        return makeLabel(item.text);
      }
      else
      {
        return createControl(item);
      }
    }

    template<typename T>
    QGraphicsItem* createCustom(const T& item)
    {
      return new oscr::CustomItem<typename T::item_type>{};
    }

    /*
    template<int N>
    constexpr void recurse(auto item)
    {
      using namespace boost::pfr;
      using namespace boost::pfr::detail;
      auto t = boost::pfr::detail::tie_as_tuple(item, size_t_<N>{});
      [&]<std::size_t... I>(std::index_sequence<I...>)
      { (this->walkLayout(sequence_tuple::get<I>(t)), ...); }
      (make_index_sequence<N>{});
    }
    */

    template<typename Item>
    void subLayout(const Item& item, score::GraphicsLayout* new_l)
    {
      auto old_l = layout;
      setupLayout(item, *new_l);
      setupItem(item, *new_l);
      layout = new_l;
      createdLayouts.push_back(new_l);

      {
        using namespace boost::pfr;
        using namespace boost::pfr::detail;
        constexpr int N = avnd::fields_count_unsafe<Item>();
        auto t = boost::pfr::detail::tie_as_tuple(item, size_t_<N>{});
        [&]<std::size_t... I>(std::index_sequence<I...>)
        { (this->walkLayout(sequence_tuple::get<I>(t)), ...); }
        (make_index_sequence<N>{});
      }

      layout = old_l;
    }

    template<typename Item>
    void walkLayout(const Item& item)
    {
      if constexpr(avnd::spacing_layout<Item>)
      {
        auto widg = new score::EmptyRectItem{layout};
        double w = 1., h = 1.;
        if constexpr(requires { Item::width(); })
          w = Item::width();
        if constexpr(requires { Item::height(); })
          h = Item::height();
        widg->setRect({0, 0, w, h});
      }
      else if constexpr(avnd::container_layout<Item>)
      {
        subLayout(item, new score::GraphicsLayout{layout});
      }
      else if constexpr(avnd::hbox_layout<Item> || avnd::group_layout<Item>)
      {
        subLayout(item, new score::GraphicsHBoxLayout{layout});
      }
      else if constexpr(avnd::vbox_layout<Item>)
      {
        subLayout(item, new score::GraphicsVBoxLayout{layout});
      }
      else if constexpr(avnd::split_layout<Item>)
      {
        subLayout(item, new score::GraphicsSplitLayout{layout});
      }
      else if constexpr(avnd::grid_layout<Item>)
      {
        if constexpr(requires { Item::columns(); })
        {
          auto new_l = new score::GraphicsGridColumnsLayout{layout};
          new_l->setColumns(Item::columns());
          subLayout(item, new_l);
        }
        else if constexpr(requires { Item::rows(); })
        {
          auto new_l = new score::GraphicsGridRowsLayout{layout};
          new_l->setRows(Item::rows());
          subLayout(item, new_l);
        }
      }
      else if constexpr(avnd::tab_layout<Item>)
      {
        auto new_l = new score::GraphicsTabLayout{layout};
        avnd::for_each_field_ref(item, [&] <typename F> (F field) {
          new_l->addTab(F::name());
        });

        subLayout(item, new_l);
      }
      else if constexpr(avnd::control_layout<Item>)
      {
        // Widget with some metadata.. FIXME
        if(auto widg = createWidget(item.control))
          setupItem(item, *widg);
      }
      else if constexpr(avnd::custom_layout<Item>)
      {
        // Widget with some metadata.. FIXME
        if(auto widg = createCustom(item))
          setupItem(item, *widg);
      }
      else if constexpr(!requires { &Item::layout; })
      {
        // Normal widget, e.g. just a const char*
        if(auto widg = createWidget(item))
          setupItem(item, *widg);
      }
      else
      {
        // Treat it like group
        subLayout(item, new score::GraphicsLayout{layout});
      }
    }
};

template <typename Info>
class LayerFactory final : public Process::LayerFactory
{
public:
  virtual ~LayerFactory()
  {
  }

private:
  std::optional<double> recommendedHeight() const noexcept override
  {
    if constexpr (requires { (double)Info::layout::height(); })
    {
      return Info::layout::height();
    }
    return Process::LayerFactory::recommendedHeight();
  }

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, oscr::ProcessModel<Info>>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, oscr::ProcessModel<Info>>::get();
  }

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& proc,
      const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    return nullptr;
  }

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::Context& context,
      QObject* parent) const final override
  {
    return nullptr;
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc,
      const Process::Context& ctx,
      QGraphicsItem* parent) const final override
  {
    using namespace score;
    auto rootItem = new score::EmptyRectItem{parent};
    if constexpr (avnd::has_ui<Info>)
    {
      LayoutBuilder<Info> b{
        *rootItem,
            ctx,
            ctx.app.interfaces<Process::PortFactoryList>(),
            proc.inlets(), proc.outlets()};
      b.walkLayout(typename Info::ui{});
      b.finalizeLayout(rootItem);
    }
    rootItem->fitChildrenRect();
    return rootItem;
  }
};

}
