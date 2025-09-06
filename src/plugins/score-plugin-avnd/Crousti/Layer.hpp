#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

#include <Control/Layout.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Painter.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <score/graphics/layouts/GraphicsSplitLayout.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/widgets/QGraphicsLineEdit.hpp>

#include <avnd/concepts/layout.hpp>

namespace oscr
{
template <typename Item>
concept recursive_container_layout
    = avnd::container_layout<Item> || avnd::hbox_layout<Item> || avnd::group_layout<Item>
      || avnd::vbox_layout<Item> || avnd::split_layout<Item> || avnd::grid_layout<Item>
      || avnd::tab_layout<Item>;

template <typename Info>
struct MessageBusUi
{
};
template <typename Info>
  requires requires { sizeof(typename Info::ui::bus); }
struct MessageBusUi<Info>
{
  typename Info::ui::bus bus;
};

template <typename Info, typename RootLayout>
struct RootItem
    : RootLayout
    , MessageBusUi<Info>
{
  using RootLayout::RootLayout;
  typename Info::ui ui;
};

template <typename Item>
static auto createRecursiveLayout(QGraphicsItem* parent)
{
  if constexpr(avnd::container_layout<Item>)
  {
    return new score::GraphicsLayout{parent};
  }
  else if constexpr(avnd::hbox_layout<Item> || avnd::group_layout<Item>)
  {
    return new score::GraphicsHBoxLayout{parent};
  }
  else if constexpr(avnd::vbox_layout<Item>)
  {
    return new score::GraphicsVBoxLayout{parent};
  }
  else if constexpr(avnd::split_layout<Item>)
  {
    return new score::GraphicsSplitLayout{parent};
  }
  else if constexpr(avnd::grid_layout<Item>)
  {
    if constexpr(requires { Item::columns(); })
    {
      return new score::GraphicsGridColumnsLayout{parent};
    }
    else if constexpr(requires { Item::rows(); })
    {
      return new score::GraphicsGridRowsLayout{parent};
    }
  }
  else if constexpr(avnd::tab_layout<Item>)
  {
    return new score::GraphicsTabLayout{parent};
  }
  else if constexpr(avnd::has_layout<Item>)
  {
    return new score::GraphicsLayout{parent};
  }
  else
  {
    // static_assert(Item::no_layout_provided);
    return new score::GraphicsVBoxLayout{parent};
  }
}

template <typename Item>
static void setupRecursiveLayout(auto* new_l)
{
  if constexpr(avnd::grid_layout<Item>)
  {
    if constexpr(requires { Item::columns(); })
    {
      new_l->setColumns(Item::columns());
    }
    else if constexpr(requires { Item::rows(); })
    {
      new_l->setRows(Item::rows());
    }
  }
  else if constexpr(avnd::tab_layout<Item>)
  {
    [=]<typename... Ts>(avnd::typelist<Ts...> args) {
      (new_l->addTab(Ts::name()), ...);
    }(avnd::as_typelist<Item>{});
  }
}

template <typename T>
struct pmf_member_type;
template <typename T, typename V>
struct pmf_member_type<V T::*>
{
  using type = V;
};

template <typename T>
using pmf_member_type_t = typename pmf_member_type<T>::type;

template <typename Info>
struct LayoutBuilder final : Process::LayoutBuilderBase
{
  using inputs_type = typename avnd::input_introspection<Info>::type;
  using outputs_type = typename avnd::output_introspection<Info>::type;
  inputs_type temp_inputs{};
  outputs_type temp_outputs{};

  typename Info::ui* rootUi{};

  template <typename Item>
  void setupControl(
      QGraphicsItem* parent, Process::ControlOutlet* inl,
      const Process::ControlLayout& lay, Item& item)
      = delete; // TODO

  template <typename Item>
  void setupControl(
      QGraphicsItem* parent, Process::ControlInlet* inl,
      const Process::ControlLayout& lay, Item& item)
  {
    if constexpr(requires { sizeof(Item::value); })
    {
      using avnd_port_type = pmf_member_type_t<decltype(item.model)>;
      avnd_port_type p;
      oscr::from_ossia_value(p, inl->value(), item.value);
      if constexpr(requires { rootUi->on_control_update(); })
      {
        QObject::connect(
            inl, &Process::ControlInlet::valueChanged, &context,
            [rui = rootUi, layout = this->layout, &item](const ossia::value& v) {
          avnd_port_type p;
          oscr::from_ossia_value(p, v, item.value);

          rui->on_control_update();
          layout->update();
        });
      }
      else
      {
        QObject::connect(
            inl, &Process::ControlInlet::valueChanged, &context,
            [layout = this->layout, &item](const ossia::value& v) {
          avnd_port_type p;
          oscr::from_ossia_value(p, v, item.value);
          layout->update();
        });
      }

      if constexpr(requires { item.set = {}; })
      {
        item.set = [inl](const auto& val) { inl->setValue(oscr::to_ossia_value(val)); };
      }
    }

    if constexpr(requires { Item::dynamic_size; })
    {
      if(auto obj = dynamic_cast<score::ResizeableItem*>(parent))
      {
        if(auto edit = qgraphicsitem_cast<score::QGraphicsLineEdit*>(lay.control))
          QObject::connect(
              edit, &score::QGraphicsLineEdit::sizeChanged, obj,
              &score::ResizeableItem::childrenSizeChanged);
      }
    }
  }

  template <typename Item>
  void createControl(Item& item, auto... member)
  {
    if constexpr(requires { ((inputs_type{}).*....*member); })
    {
      int index = avnd::index_in_struct(temp_inputs, member...);
      auto& proc = static_cast<const ProcessModel<Info>&>(this->proc);
      auto ports = proc.avnd_input_idx_to_model_ports(index);
      for(auto p : ports)
      {
        auto [port, qitem] = makeInlet(p);
        {
          SCORE_ASSERT(port);
          SCORE_ASSERT(qitem.container);
          setupControl(this->layout, port, qitem, item);
          setupItem(item, *qitem.container);
        }
      }
    }
    else if constexpr(requires { ((outputs_type{}).*....*member); })
    {
      int index = avnd::index_in_struct(temp_outputs, member...);
      auto& proc = static_cast<const ProcessModel<Info>&>(this->proc);
      auto ports = proc.avnd_output_idx_to_model_ports(index);
      for(auto p : ports)
      {
        auto [port, qitem] = makeOutlet(p);
        {
          SCORE_ASSERT(port);
          SCORE_ASSERT(qitem.container);
          setupControl(this->layout, port, qitem, item);
          setupItem(item, *qitem.container);
        }
      }
    }
    else
    {
      static_assert(sizeof...(member) < 0, "not_a_member_of_inputs_or_outputs");
    }
  }

  template <typename Item, typename T>
  void createWidget(Item& it, const T& member)
  {
    if constexpr(requires {
                   { member } -> std::convertible_to<std::string_view>;
                 })
    {
      auto res = makeLabel(member);
      setupItem(it, *res);
    }
    else if constexpr(requires {
                        { member.text } -> std::convertible_to<std::string_view>;
                      })
    {
      auto res = makeLabel(member.text);
      setupItem(it, *res);
    }
    else
    {
      createControl(it, member);
    }
  }

  template <typename Item, typename... T>
    requires(sizeof...(T) > 1)
  void createWidget(Item& item, T... recursive_members)
  {
    createControl(item, recursive_members...);
  }

  template <typename Item>
  void createCustom(Item& item)
  {
    static_assert(!requires { item.transaction; });
    auto res = new oscr::CustomItem<Item&>{item};
    setupItem(item, *res);
  }

  template <typename Item>
  void createCustomControl(Item& item, auto... member)
  {
    if constexpr(requires { ((inputs_type{}).*....*member); })
    {
      int index = avnd::index_in_struct(temp_inputs, member...);

      auto& proc = static_cast<const ProcessModel<Info>&>(this->proc);
      auto ports = proc.avnd_input_idx_to_model_ports(index);
      for(auto p : ports)
      {
        if(auto* port = qobject_cast<Process::ControlInlet*>(p))
        {
          auto qitem = new oscr::CustomControl<Item&>{item, *port, this->doc};
          Process::ControlLayout lay{.container = qitem};
          if(auto* f = portFactory.get(port->concreteKey()))
          {
            lay.port_item = f->makePortItem(*port, this->doc, this->layout, &context);
          }
          setupControl(this->layout, port, lay, item);
          setupItem(item, *qitem);
        }
      }
    }
    else if constexpr(requires { ((outputs_type{}).*....*member); })
    {
      int index = avnd::index_in_struct(temp_outputs, member...);

      auto& proc = static_cast<const ProcessModel<Info>&>(this->proc);
      auto ports = proc.avnd_output_idx_to_model_ports(index);
      for(auto p : ports)
      {
        if(auto* port = qobject_cast<Process::ControlOutlet*>(p))
        {
          auto qitem = new oscr::CustomControl<Item&>{item, *port, this->doc};
          Process::ControlLayout lay{.container = qitem};
          if(auto* f = portFactory.get(port->concreteKey()))
          {
            lay.port_item = f->makePortItem(*port, this->doc, this->layout, &context);
          }
          setupControl(this->layout, port, lay, item);
          setupItem(item, *qitem);
        }
      }
    }
    else
    {
      static_assert(sizeof...(member) < 0, "not_a_member_of_inputs_or_outputs");
    }
  }

  template <typename Item>
  void subLayout(Item& item, score::GraphicsLayout* new_l, auto... recursive_members)
  {
    auto old_l = layout;
    setupLayout(item, *new_l);
    setupItem(item, *new_l);
    layout = new_l;
    createdLayouts.push_back(new_l);

    {
      using namespace boost::pfr;
      using namespace boost::pfr::detail;
      static constexpr int N = boost::pfr::tuple_size_v<Item>;
      auto t = boost::pfr::detail::tie_as_tuple(item, size_t_<N>{});
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        (this->walkLayout(sequence_tuple::get<I>(t), recursive_members...), ...);
      }(std::make_index_sequence<N>{});
    }

    layout = old_l;
  }

  template <typename Item>
  auto initRecursiveLayout()
  {
    auto new_l = createRecursiveLayout<Item>(this->layout);
    setupRecursiveLayout<Item>(new_l);
    return new_l;
  }

  template <typename Item>
  void walkLayout(Item& item, auto... recursive_members)
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
    else if constexpr(recursive_container_layout<Item>)
    {
      subLayout(item, initRecursiveLayout<Item>(), recursive_members...);
    }
    else if constexpr(avnd::control_layout<Item>)
    {
      // Widget with some metadata.. FIXME
      // Auto-generated item for a control
      createWidget(item, recursive_members..., item.model);
    }
    else if constexpr(avnd::custom_control_layout<Item>)
    {
      // Widget with some metadata.. FIXME
      // Custom-drawn item for a control
      createCustomControl(item, recursive_members..., item.model);
    }
    else if constexpr(avnd::custom_layout<Item>)
    {
      // Widget with some metadata.. FIXME
      // This is just a cosmetic item without behaviour or control attached
      createCustom(item);
    }
    else if constexpr(avnd::recursive_group_layout<Item>)
    {
      walkLayout(item.ui, recursive_members..., item.group);
    }
    else if constexpr(avnd::dynamic_controls<Item>)
    {
      walkLayout(item.ui, recursive_members..., item.group);
    }
    else if constexpr(avnd::has_layout<Item>)
    {
      // Treat it like group
      subLayout(item, initRecursiveLayout<Item>(), recursive_members...);
    }
    else
    {
      // Normal widget, e.g. just a const char*
      createWidget(item, item);
    }
  }
};

template <typename Info>
class LayerFactory final : public Process::LayerFactory
{
public:
  virtual ~LayerFactory() { }

private:
  std::optional<double> recommendedHeight() const noexcept override
  {
    if constexpr(requires { (double)Info::layout::height(); })
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
      const Process::ProcessModel& proc, const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    return nullptr;
  }

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm, Process::LayerView* v,
      const Process::Context& context, QObject* parent) const final override
  {
    return nullptr;
  }

  template <typename Item>
  static void init_bus(ProcessModel<Info>& proc, Item& item)
  {
    auto ptr = &item;
    if constexpr(avnd::has_gui_to_processor_bus<Info>)
    {
      // ui -> engine
      ptr->bus.send_message = MessageBusSender{proc.from_ui};
    }

    if constexpr(avnd::has_processor_to_gui_bus<Info>)
    {
      // engine -> ui
      proc.to_ui = [ptr = QPointer{ptr}](QByteArray mess) {
        // FIXME this is not enough as the message may be sent from another thread?
        if(!ptr)
          return;

        if constexpr(requires { ptr->bus.process_message(); })
        {
          ptr->bus.process_message();
        }
        else if constexpr(requires { ptr->bus.process_message(ptr->ui); })
        {
          ptr->bus.process_message(ptr->ui);
        }
        else if constexpr(requires { ptr->bus.process_message(ptr->ui, {}); })
        {
          std::decay_t<avnd::second_argument<&Info::ui::bus::process_message>> arg;
          MessageBusReader b{mess};
          b(arg);
          ptr->bus.process_message(ptr->ui, std::move(arg));
        }
        else
        {
          ptr->bus.process_message(ptr->ui, {});
        }
      };
    }

    if_possible(ptr->bus.init(ptr->ui));
  }

  auto makeItemImpl(ProcessModel<Info>& proc, QGraphicsItem* parent) const noexcept
  {
    using ui_type = typename Info::ui;
    using root_layout_type
        = std::remove_cvref_t<decltype(*createRecursiveLayout<ui_type>(nullptr))>;

    auto new_l = new RootItem<Info, root_layout_type>{parent};
    setupRecursiveLayout<ui_type>(new_l);
    if constexpr(requires { sizeof(typename Info::ui::bus); })
      init_bus(proc, *new_l);

    if constexpr(requires { new_l->ui.start(); })
    {
      QObject::connect(&proc, &Process::ProcessModel::startExecution, new_l, [new_l] {
        new_l->ui.start();
      });
    }
    if constexpr(requires { new_l->ui.stop(); })
    {
      QObject::connect(&proc, &Process::ProcessModel::stopExecution, new_l, [new_l] {
        new_l->ui.stop();
      });
    }
    if constexpr(requires { new_l->ui.reset(); })
    {
      QObject::connect(&proc, &Process::ProcessModel::resetExecution, new_l, [new_l] {
        new_l->ui.reset();
      });
    }
    return new_l;
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc, const Process::Context& ctx,
      QGraphicsItem* parent) const final override
  {
    using namespace score;
    auto& process = static_cast<const ProcessModel<Info>&>(proc);

    auto rootItem = makeItemImpl(const_cast<ProcessModel<Info>&>(process), parent);

    auto recreate = [parent, &proc, &ctx, rootItem] {
      LayoutBuilder<Info> b{
          *rootItem,     proc,
          ctx,           ctx.app.interfaces<Process::PortFactoryList>(),
          proc.inlets(), proc.outlets(),
      };
      b.rootUi = &rootItem->ui;
      b.layout = parent;

      b.subLayout(rootItem->ui, rootItem);

      b.finalizeLayout(rootItem);

      rootItem->fitChildrenRect();

      if_possible(b.rootUi->on_control_update());
    };

    QObject::connect(&proc, &Process::ProcessModel::inletsChanged, rootItem, [=]() {
      auto cld = rootItem->childItems();
      for(auto item : cld)
      {
        delete item;
      }
      recreate();
    });
    QObject::connect(&proc, &Process::ProcessModel::outletsChanged, rootItem, [=]() {
      auto cld = rootItem->childItems();
      for(auto item : cld)
      {
        delete item;
      }
      recreate();
    });

    recreate();
    return rootItem;
  }
};

}
