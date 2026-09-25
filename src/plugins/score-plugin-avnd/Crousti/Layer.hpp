#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

#include <Control/Layout.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/ControlViews.hpp>
#include <Crousti/Painter.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <score/graphics/layouts/GraphicsSplitLayout.hpp>
#include <score/graphics/layouts/GraphicsStripDetailLayout.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/layouts/LayoutSelection.hpp>
#include <score/graphics/widgets/QGraphicsLineEdit.hpp>
#include <score/tools/File.hpp>

#include <avnd/common/aggregates.hpp>
#include <avnd/common/enum_reflection.hpp>

#include <QFileInfo>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>
#include <QFontMetricsF>
#include <avnd/concepts/file_port.hpp>
#include <avnd/concepts/layout.hpp>
#include <avnd/concepts/midifile.hpp>
#include <avnd/concepts/soundfile.hpp>
#include <halp/layout.hpp>

namespace oscr
{
template <typename Item>
concept recursive_container_layout
    = avnd::container_layout<Item> || avnd::hbox_layout<Item> || avnd::group_layout<Item>
      || avnd::vbox_layout<Item> || avnd::split_layout<Item> || avnd::grid_layout<Item>
      || avnd::tab_layout<Item> || avnd::section_layout<Item>
      || avnd::table_layout<Item> || avnd::strip_detail_layout<Item>;

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
  else if constexpr(avnd::section_layout<Item>)
  {
    return new score::GraphicsSectionLayout{parent};
  }
  else if constexpr(avnd::table_layout<Item>)
  {
    return new score::GraphicsTableLayout{parent};
  }
  else if constexpr(avnd::strip_detail_layout<Item>)
  {
    return new score::GraphicsStripDetailLayout{parent};
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
  else if constexpr(avnd::section_layout<Item>)
  {
    // Defaults a plugin can override with halp_meta(padding / spacing)
    new_l->setPadding(6.);
    new_l->setSpacing(4.);
    if constexpr(requires { Item::name(); })
    {
      const std::string_view name = Item::name();
      new_l->setTitle(QString::fromUtf8(name.data(), qsizetype(name.size())));
    }
  }
  else if constexpr(avnd::table_layout<Item>)
  {
    // Defaults a plugin can override with halp_meta(padding / spacing)
    new_l->setPadding(4.);
    new_l->setSpacing(6.);
    if constexpr(requires { Item::name(); })
    {
      const std::string_view name = Item::name();
      new_l->setTitle(QString::fromUtf8(name.data(), qsizetype(name.size())));
    }
    if constexpr(requires { Item::columns(); })
    {
      QStringList titles;
      for(std::string_view t : Item::columns())
        titles.push_back(QString::fromUtf8(t.data(), qsizetype(t.size())));
      new_l->setColumnTitles(std::move(titles));
    }
  }
  else if constexpr(avnd::tab_layout<Item>)
  {
    if constexpr(avnd::tag_hide_tabs<Item>)
      new_l->setTabBarVisible(false);
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

template <typename T>
struct SetGUIValue
{
  const score::DocumentContext& ctx;
  void operator()(const ossia::value& v, auto& dst) const
  {
    T p;
    oscr::from_ossia_value(p, v, dst);
  }
};

// File & folder ports: paths are stored in the document as templates
// (<PROJECT>:..., <LIBRARY>:..., or document-relative); resolve them so the
// ui items always receive a usable absolute path.
template <typename T>
  requires(
      avnd::soundfile_port<T> || avnd::midifile_port<T> || avnd::file_port<T>
      || requires { T::widget::folder; })
struct SetGUIValue<T>
{
  const score::DocumentContext& ctx;
  void operator()(const ossia::value& v, auto& dst) const
  {
    T p;
    if(auto str = v.target<std::string>(); str && !str->empty())
      oscr::from_ossia_value(
          p,
          ossia::value{
              score::locateFilePath(QString::fromStdString(*str), ctx).toStdString()},
          dst);
    else
      oscr::from_ossia_value(p, v, dst);
  }
};

// The inverse of SetGUIValue: an item hands back the absolute path it was
// given, and the document must keep storing a template, or the project stops
// being relocatable. Non-file ports pass through untouched.
template <typename T>
struct GetGUIValue
{
  const score::DocumentContext& ctx;
  ossia::value operator()(ossia::value v) const { return v; }
};

template <typename T>
  requires(
      avnd::soundfile_port<T> || avnd::midifile_port<T> || avnd::file_port<T>
      || requires { T::widget::folder; })
struct GetGUIValue<T>
{
  const score::DocumentContext& ctx;
  ossia::value operator()(ossia::value v) const
  {
    if(auto str = v.target<std::string>(); str && !str->empty())
      return ossia::value{
          score::relativizeFilePath(QString::fromStdString(*str), ctx).toStdString()};
    return v;
  }
};

//! Where an enumerator sits among its enum's entries, which is the order an
//! enumeration port lists them in: its index there, whatever its value (an
//! enum's values need not be 0, 1, 2...). Other values are used as they are.
template <typename V>
static constexpr int entryIndex(V v) noexcept
{
  if constexpr(std::is_enum_v<V>)
  {
    constexpr auto values = avnd::enum_values<V>();
    for(std::size_t i = 0; i < values.size(); ++i)
      if(values[i] == v)
        return int(i);
    return -1;
  }
  else
  {
    return static_cast<int>(v);
  }
}

//! A layout item's presentation (halp::look), in score's terms.
template <typename Item>
static Process::ControlPresentation controlPresentation()
{
  Process::ControlPresentation p;
  if constexpr(requires {
                 { Item::presentation } -> std::convertible_to<halp::look>;
               })
  {
    constexpr halp::look look = Item::presentation;
    if(look.label[0] != 0)
      p.label = QString::fromUtf8(look.label);
    p.labelVisible = !look.hide_label;
    if(look.size == halp::control_size::compact)
      p.size = score::ControlSize::Small;
    else if(look.size == halp::control_size::large)
      p.size = score::ControlSize::Large;
    p.valueOnHover = look.value == halp::value_display::hover;
    if(look.widget == halp::control_widget::knob)
      p.widget = Process::ControlPresentation::Widget::Knob;
    else if(look.widget == halp::control_widget::combo)
      p.widget = Process::ControlPresentation::Widget::Combo;
  }
  return p;
}

template <typename Info>
struct LayoutBuilder final : Process::LayoutBuilderBase
{
  using inputs_type = typename avnd::input_introspection<Info>::type;
  using outputs_type = typename avnd::output_introspection<Info>::type;
  inputs_type temp_inputs{};
  outputs_type temp_outputs{};

  typename Info::ui* rootUi{};

  //! How many tables with column titles the walk is inside: the titles name
  //! the controls, which then do not show their own.
  int tableDepth{};

  //! Selections shared by the layouts naming them (halp_meta(selection, ...)),
  //! and the one each selecting table drives
  std::map<std::string, std::shared_ptr<score::LayoutSelection>, std::less<>> selections;
  std::map<score::GraphicsTableLayout*, std::shared_ptr<score::LayoutSelection>>
      tableSelections;

  std::shared_ptr<score::LayoutSelection> selectionNamed(std::string_view name)
  {
    auto it = selections.find(name);
    if(it == selections.end())
    {
      auto sel = std::make_shared<score::LayoutSelection>();
      // The ui is rebuilt when the inlets change: keep what was selected on
      // the layer's root item, which outlives the rebuilds
      const QByteArray key = "layoutSelection:" + QByteArray{name.data(), qsizetype(name.size())};
      sel->current = context.property(key.constData()).toInt();
      sel->listeners.push_back(
          [ctx = &context, key](int i) { ctx->setProperty(key.constData(), i); });
      it = selections.emplace(std::string{name}, std::move(sel)).first;
    }
    return it->second;
  }

  //! Port_T is Process::ControlInlet or Process::ControlOutlet: a control a
  //! layout names is set up the same way whichever side it is on, the widget
  //! following the port's value.
  template <typename Port_T, typename Item>
  void setupControl(
      QGraphicsItem* parent, Port_T* port, const Process::ControlLayout& lay, Item& item)
  {
    if constexpr(requires { sizeof(Item::value); })
    {
      using avnd_port_type = pmf_member_type_t<decltype(item.model)>;
      SetGUIValue<avnd_port_type>{doc}(port->value(), item.value);
      if constexpr(requires { rootUi->on_control_update(); })
      {
        QObject::connect(
            port, &Port_T::valueChanged, &context,
            [rui = rootUi, layout = this->layout, &item,
             &ctx = static_cast<const score::DocumentContext&>(this->doc)](
                const ossia::value& v) {
          SetGUIValue<avnd_port_type>{ctx}(v, item.value);

          rui->on_control_update();
          layout->update();
        });
      }
      else
      {
        QObject::connect(
            port, &Port_T::valueChanged, &context,
            [layout = this->layout, &item,
             &ctx = static_cast<const score::DocumentContext&>(this->doc)](
                const ossia::value& v) {
          SetGUIValue<avnd_port_type>{ctx}(v, item.value);
          layout->update();
        });
      }

      if constexpr(requires { item.set = {}; })
      {
        item.set = [port, &ctx = static_cast<const score::DocumentContext&>(this->doc)](
                       const auto& val) {
          port->setValue(GetGUIValue<avnd_port_type>{ctx}(oscr::to_ossia_value(val)));
        };
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
    if constexpr(requires { Item::width; })
    {
      if(auto edit = qgraphicsitem_cast<score::QGraphicsLineEdit*>(lay.control))
        edit->setTextWidth(Item::width());
    }
  }

  //! halp::enabled_when / visible_when: follow the value of the control the
  //! layout depends on. Enums and combo boxes match on the entry selected
  //! (see entryIndex); other controls on their numeric value.
  template <typename Item>
  void setupCondition(score::GraphicsLayout& lay, auto... recursive_members)
  {
    const int index = avnd::index_in_struct(
        temp_inputs, recursive_members..., Item::condition_control);
    auto& model = static_cast<const ProcessModel<Info>&>(this->proc);
    for(auto* base : model.avnd_input_idx_to_model_ports(index))
    {
      auto* port = qobject_cast<Process::ControlInlet*>(base);
      if(!port)
        continue;

      auto matches = [port](const ossia::value& v) {
        int idx = -1;
        if(auto* e = qobject_cast<Process::Enum*>(port))
          idx = e->indexOfValue(v);
        else if(auto* c = qobject_cast<Process::ComboBox*>(port))
          idx = c->indexOfValue(v);
        const bool is_choice = qobject_cast<Process::Enum*>(port)
                               || qobject_cast<Process::ComboBox*>(port);
        for(auto expected : Item::condition_values())
        {
          if(is_choice ? idx == entryIndex(expected)
                       : ossia::convert<double>(v) == static_cast<double>(expected))
            return true;
        }
        return false;
      };
      auto apply = [&lay, matches](const ossia::value& v) {
        const bool on = matches(v);
        if constexpr(Item::condition_hides)
        {
          lay.setVisible(on);
        }
        else
        {
          lay.setEnabled(on);
          lay.setOpacity(on ? 1. : 0.35);
        }
      };
      apply(port->value());
      // Scoped to the layout item, which is rebuilt when the inlets change
      QObject::connect(port, &Process::ControlInlet::valueChanged, &lay, apply);
      break;
    }
  }

  template <typename Item>
  Process::ControlPresentation presentationFor() const
  {
    auto p = controlPresentation<Item>();
    if(tableDepth > 0 && !p.label)
      p.labelVisible = false;
    return p;
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
        auto [port, qitem] = makeInlet(p, presentationFor<Item>());
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
        auto [port, qitem] = makeOutlet(p, presentationFor<Item>());
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

  //! A file shown in a strip cell: files dropped on the cell load there, as
  //! if chosen with the port's own file chooser
  //! The strip cell being built, when in the layout of a summary
  score::GraphicsStripCell* enclosingCell() const
  {
    score::GraphicsStripCell* cell{};
    for(auto* item = this->layout; item && !cell; item = item->parentItem())
      cell = dynamic_cast<score::GraphicsStripCell*>(item);
    return cell;
  }

  void acceptFileDrops(Process::FileChooserBase& port)
  {
    auto* cell = enclosingCell();
    if(!cell)
      return;

    // "Sound files (*.wav *.flac)" -> wav, flac. None: any file.
    QStringList extensions;
    static const QRegularExpression pattern{QStringLiteral("\\*\\.([A-Za-z0-9]+)")};
    for(auto it = pattern.globalMatch(port.filters()); it.hasNext();)
      extensions.push_back(it.next().captured(1).toLower());

    auto file_of = [extensions](const QMimeData& data) -> QString {
      for(const QUrl& url : data.urls())
      {
        if(!url.isLocalFile())
          continue;
        const QString path = url.toLocalFile();
        if(extensions.isEmpty()
           || extensions.contains(QFileInfo{path}.suffix().toLower()))
          return path;
      }
      return {};
    };
    cell->setDropHandler(
        [file_of](const QMimeData& data) { return !file_of(data).isEmpty(); },
        [file_of, &port, &doc = this->doc](const QMimeData& data) {
      const QString path = file_of(data);
      if(path.isEmpty())
        return;
      CommandDispatcher<>{doc.commandStack}.submit<Process::SetControlValue>(
          port, ossia::value{score::relativizeFilePath(path, doc).toStdString()});
    });
  }

  //! What a display shows for a value: an entry's name, or a number
  static QString displayText(
      const Process::ControlInlet& port, const ossia::value& v,
      const QFontMetricsF& metrics, qreal max_width = 72.)
  {
    if(auto* c = qobject_cast<const Process::ComboBox*>(&port))
    {
      const int i = c->indexOfValue(v);
      if(i >= 0 && i < std::ssize(c->alternatives))
        return c->alternatives[i].first;
    }
    else if(auto* e = qobject_cast<const Process::Enum*>(&port))
    {
      const int i = e->indexOfValue(v);
      if(i >= 0 && i < std::ssize(e->values))
        return e->values[i];
    }
    if(auto str = v.target<std::string>())
    {
      QString text = QString::fromStdString(*str);
      // A file shows as its name, without its folders or extension
      if(text.contains(QLatin1Char('/')) || text.contains(QLatin1Char('\\')))
        text = QFileInfo{text}.completeBaseName();
      else if(text.isEmpty())
        text = QStringLiteral("\u2014");
      // Short enough for a cell of a strip
      return metrics.elidedText(text, Qt::ElideRight, max_width);
    }
    return QString::number(ossia::convert<double>(v), 'g', 4);
  }

  template <typename Item>
  void createDisplay(Item& item, auto... member)
  {
    if constexpr(requires { ((inputs_type{}).*....*member); })
    {
      const int index = avnd::index_in_struct(temp_inputs, member...);
      auto& proc = static_cast<const ProcessModel<Info>&>(this->proc);
      for(auto* p : proc.avnd_input_idx_to_model_ports(index))
      {
        auto* port = qobject_cast<Process::ControlInlet*>(p);
        if(!port)
          continue;
        if(auto* file = qobject_cast<Process::FileChooserBase*>(port))
          acceptFileDrops(*file);
        if constexpr(
            requires { Item::style; } && Item::style == halp::display_style::bar)
        {
          auto* bar = new oscr::ValueBarItem;
          auto show = [bar, port](const ossia::value& v) {
            bar->setValue(oscr::normalizedValue(*port, v));
          };
          show(port->value());
          QObject::connect(port, &Process::ControlInlet::valueChanged, bar, show);
          setupItem(item, *bar);
        }
        else if constexpr(
            requires { Item::style; } && Item::style == halp::display_style::title)
        {
          // No item: the cell elides the text to its own width
          if(auto* cell = enclosingCell())
          {
            auto show = [cell, port, metrics = QFontMetricsF{score::Skin::instance().Medium8Pt}](
                            const ossia::value& v) {
              const auto str = v.target<std::string>();
              cell->setShownTitle(
                  str && !str->empty() ? displayText(*port, v, metrics, 1e6) : QString{});
            };
            show(port->value());
            QObject::connect(port, &Process::ControlInlet::valueChanged, cell, show);
          }
        }
        else
        {
          auto* text
              = new score::SimpleTextItem{score::Skin::instance().Base4.main, nullptr};
          // Measured once, not on each change of the value
          auto show = [text, port, metrics = QFontMetricsF{score::Skin::instance().Medium8Pt}](
                          const ossia::value& v) {
            text->setText(displayText(*port, v, metrics));
          };
          show(port->value());
          QObject::connect(port, &Process::ControlInlet::valueChanged, text, show);
          setupItem(item, *text);
        }
        break;
      }
    }
  }

  void addControlInlet(std::vector<Process::ControlInlet*>& ports, auto... member)
  {
    if constexpr(requires { ((inputs_type{}).*....*member); })
    {
      const int index = avnd::index_in_struct(temp_inputs, member...);
      auto& proc = static_cast<const ProcessModel<Info>&>(this->proc);
      for(auto* p : proc.avnd_input_idx_to_model_ports(index))
      {
        if(auto* port = qobject_cast<Process::ControlInlet*>(p))
        {
          ports.push_back(port);
          return;
        }
      }
    }
    else
    {
      static_assert(sizeof...(member) < 0, "not_a_member_of_inputs");
    }
  }

  template <typename Item>
  void createCustomMultiControl(Item& item, auto... recursive_members)
  {
    std::vector<Process::ControlInlet*> ports;
    std::apply(
        [&](auto... models) { (addControlInlet(ports, recursive_members..., models), ...); },
        Item::models());
    auto* qitem = new oscr::CustomMultiControl<Item&>{item, std::move(ports), this->doc};
    setupItem(item, *qitem);
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

    // A named row of a table starts with its name
    if(auto table = dynamic_cast<score::GraphicsTableLayout*>(old_l))
    {
      if constexpr(requires { Item::name(); })
      {
        makeLabel(Item::name())->setParentItem(new_l);
        table->setRowTitles(true);
      }
      if(auto row = dynamic_cast<score::GraphicsSelectableRow*>(new_l))
      {
        auto it = tableSelections.find(table);
        if(it != tableSelections.end())
        {
          // The row was just added: its index is the number of rows before it
          int index = -1;
          for(auto* child : table->childItems())
            if(dynamic_cast<score::GraphicsSelectableRow*>(child))
              ++index;
          auto sel = it->second;
          row->onActivated = [sel, index] { sel->select(index); };
          sel->listen([row, index](int i) { row->setSelected(i == index); });
        }
      }
    }
    if constexpr(avnd::table_layout<Item> && requires { Item::columns(); })
      ++tableDepth;

    if constexpr(requires { Item::condition_control; })
      setupCondition<Item>(*new_l, recursive_members...);

    if constexpr(avnd::table_layout<Item>)
    {
      forEachMember(item, [](auto& row) {
        static_assert(
            avnd::hbox_layout<std::remove_cvref_t<decltype(row)>>,
            "The members of a halp table are its rows: structs with "
            "halp_meta(layout, halp::layouts::hbox)");
      });
    }

    // Rows of a table naming a selection select their unit when pressed
    if constexpr(avnd::table_layout<Item> && requires { Item::selection(); })
    {
      auto* table = static_cast<score::GraphicsTableLayout*>(new_l);
      table->setRowsSelectable(true);
      tableSelections[table] = selectionNamed(Item::selection());
    }

    if constexpr(avnd::tab_layout<Item> && requires { Item::model; })
    {
      auto* tabs = static_cast<score::GraphicsTabLayout*>(new_l);
      const int index
          = avnd::index_in_struct(temp_inputs, recursive_members..., Item::model);

      // Pages may say which values of the model show them, so that several
      // values share a page: static constexpr auto when() { return std::array{...}; }
      // Without it, value i shows page i.
      std::vector<int> page_of;
      {
        int page = 0;
        forEachMember(item, [&](auto& p) {
          using page_type = std::remove_cvref_t<decltype(p)>;
          if constexpr(requires { page_type::when(); })
          {
            for(auto v : page_type::when())
            {
              const int i = entryIndex(v);
              if(i < 0)
                continue;
              if(i >= std::ssize(page_of))
                page_of.resize(i + 1, -1);
              page_of[i] = page;
            }
          }
          ++page;
        });
      }
      auto to_page = [page_of](int i) {
        return (i >= 0 && i < std::ssize(page_of) && page_of[i] >= 0) ? page_of[i] : i;
      };
      auto to_value_index = [page_of](int page) {
        for(int i = 0; i < std::ssize(page_of); i++)
          if(page_of[i] == page)
            return i;
        return page;
      };
      auto& model = static_cast<const ProcessModel<Info>&>(this->proc);
      for(auto* base : model.avnd_input_idx_to_model_ports(index))
      {
        if(auto* port = dynamic_cast<Process::Enum*>(base))
        {
          auto select = [tabs, port, to_page](const ossia::value& value) {
            if(const int i = port->indexOfValue(value); i >= 0)
              tabs->setCurrentIndex(to_page(i));
          };
          select(port->value());
          QObject::connect(port, &Process::ControlInlet::valueChanged, tabs, select);
          if constexpr(!avnd::tag_hide_tabs<Item>)
            tabs->onCurrentIndexChanged
                = [port, to_value_index, &doc = this->doc](int i) {
              const ossia::value value = port->valueAtIndex(to_value_index(i));
              if(value.valid() && port->value() != value)
                CommandDispatcher<>{doc.commandStack}.submit<Process::SetControlValue>(
                    *port, value);
            };
          break;
        }
        if(auto* port = dynamic_cast<Process::ComboBox*>(base))
        {
          auto select = [tabs, port, to_page](const ossia::value& value) {
            if(const int i = port->indexOfValue(value); i >= 0)
              tabs->setCurrentIndex(to_page(i));
          };
          select(port->value());
          QObject::connect(port, &Process::ControlInlet::valueChanged, tabs, select);
          if constexpr(!avnd::tag_hide_tabs<Item>)
            tabs->onCurrentIndexChanged
                = [port, to_value_index, &doc = this->doc](int i) {
              const ossia::value value = port->valueAtIndex(to_value_index(i));
              if(value.valid() && port->value() != value)
                CommandDispatcher<>{doc.commandStack}.submit<Process::SetControlValue>(
                    *port, value);
            };
          break;
        }
      }
    }

    if constexpr(avnd::strip_detail_layout<Item>)
    {
      auto& strip = *static_cast<score::GraphicsStripDetailLayout*>(new_l);
      createdLayouts.push_back(&strip.strip());
      // Each page: a cell in the strip with its summary, then the page itself
      auto page = [&](auto& p) {
        auto* cell = new score::GraphicsStripCell{nullptr};
        using page_type = std::remove_cvref_t<decltype(p)>;
        if constexpr(requires { page_type::name(); })
        {
          const std::string_view name = page_type::name();
          cell->setTitle(QString::fromUtf8(name.data(), qsizetype(name.size())));
        }
        strip.addCell(cell);
        createdLayouts.push_back(cell);
        // The page's summary is one of its members, so that what the cell
        // shows (buttons included) lives as long as the ui does
        if constexpr(requires { p.summary; })
        {
          layout = cell;
          walkMembers(p.summary, recursive_members..., p.group);
          layout = new_l;
        }
        this->walkLayout(p, recursive_members...);
      };
      forEachMember(item, page);

      // Another layout (e.g. a table's rows) may choose the page instead
      if constexpr(requires { Item::selection(); })
      {
        auto sel = selectionNamed(Item::selection());
        sel->listen([&strip](int i) { strip.setCurrentIndex(i); });
        strip.onCurrentIndexChanged = [sel](int i) { sel->select(i); };
        if constexpr(avnd::tag_hide_tabs<Item>)
          strip.setStripVisible(false);
      }
    }
    else
    {
      walkMembers(item, recursive_members...);
    }

    if constexpr(avnd::table_layout<Item> && requires { Item::columns(); })
      --tableDepth;
    layout = old_l;
  }

  template <typename Item, typename F>
  static void forEachMember(Item& item, F&& f)
  {
#if AVND_USE_BOOST_PFR
    static constexpr int N = avnd::pfr::tuple_size_v<Item>;
    auto t = avnd::pfr::detail::tie_as_tuple(item);
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      using namespace std;
      using namespace avnd::pfr;
      (f(get<I>(t)), ...);
    }(std::make_index_sequence<N>{});
#else
    auto&& [... members] = item;
    (f(members), ...);
#endif
  }

  template <typename Item>
  void walkMembers(Item& item, auto... recursive_members)
  {
    forEachMember(item, [&](auto& m) { this->walkLayout(m, recursive_members...); });
  }

  template <typename Item>
  auto initRecursiveLayout()
  {
    if constexpr(avnd::hbox_layout<Item>)
    {
      // The rows of a table driving a selection can be selected
      if(auto table = dynamic_cast<score::GraphicsTableLayout*>(this->layout);
         table && tableSelections.contains(table))
      {
        score::GraphicsHBoxLayout* row = new score::GraphicsSelectableRow{this->layout};
        setupRecursiveLayout<Item>(row);
        return row;
      }
    }
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
      // Static (halp_meta) or per-instance, like halp::spacing{.width, .height}
      double w = 1., h = 1.;
      if constexpr(requires { Item::width(); })
        w = Item::width();
      else if constexpr(requires { item.width; })
        w = item.width;
      if constexpr(requires { Item::height(); })
        h = Item::height();
      else if constexpr(requires { item.height; })
        h = item.height;
      widg->setRect({0, 0, w, h});
    }
    else if constexpr(recursive_container_layout<Item>)
    {
      subLayout(item, initRecursiveLayout<Item>(), recursive_members...);
    }
    else if constexpr(avnd::control_layout<Item> && requires { Item::display_only; })
    {
      // Read-only view of a value, e.g. in a strip cell
      createDisplay(item, recursive_members..., item.model);
    }
    else if constexpr(avnd::control_layout<Item>)
    {
      // Widget with some metadata.. FIXME
      // Auto-generated item for a control
      createWidget(item, recursive_members..., item.model);
    }
    else if constexpr(avnd::custom_multi_control_layout<Item>)
    {
      // Custom-drawn item bound to several controls
      createCustomMultiControl(item, recursive_members...);
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
      // Spacing inside an avendish ui is declared by the plugin (spacing
      // items, padding), so nested layouts fit their content exactly.
      b.marginOnNestedLayouts = false;

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
