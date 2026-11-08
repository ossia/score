#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/model/Skin.hpp>

#include <ossia/network/domain/domain.hpp>

#include <Crousti/Painter.hpp>
#include <Crousti/UiEdits.hpp>
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QPainter>

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

namespace oscr
{
//! halp::display_style::bar
class ValueBarItem final
    : public QObject
    , public QGraphicsItem
{
public:
  explicit ValueBarItem(QGraphicsItem* parent = nullptr)
      : QGraphicsItem{parent}
  {
  }

  void setValue(double normalized)
  {
    // Values stream in at the control rate: repaint only on a change
    normalized = std::clamp(normalized, 0., 1.);
    if(normalized == m_value)
      return;
    m_value = normalized;
    update();
  }

  QRectF boundingRect() const override { return {0., 0., 52., 4.}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    auto& skin = score::Skin::instance();
    const QRectF r = boundingRect();
    painter->fillRect(r, skin.Background2.darker.brush);
    painter->fillRect(QRectF{r.x(), r.y(), r.width() * m_value, r.height()}, skin.Base4);
  }

private:
  double m_value{};
};

//! The number a control stands for: for a time chooser's {x, mode}, seconds.
//! The UI has no tempo: a synced value is its length at 120 BPM, which keeps
//! the proportions between synced values.
inline double scalarValue(const ossia::value& v)
{
  if(auto t = v.target<ossia::vec2f>())
    return (*t)[1] != 0.f ? (*t)[0] * 2. : (*t)[0]; // a whole note: 2 s
  return ossia::convert<double>(v);
}

//! A control's value, normalized over its range
inline double normalizedValue(const Process::ControlInlet& port, const ossia::value& v)
{
  const auto& dom = port.domain().get();
  const auto lo = ossia::get_min(dom);
  const auto hi = ossia::get_max(dom);
  if(!lo.valid() || !hi.valid())
    return std::clamp(scalarValue(v), 0., 1.);
  const double l = scalarValue(lo), h = scalarValue(hi);
  // A time chooser's domain is {min, 0} .. {max, 1}: its x is the range
  if(auto a = lo.target<ossia::vec2f>())
    if(auto b = hi.target<ossia::vec2f>())
    {
      const double tl = (*a)[0], th = (*b)[0];
      return th == tl ? 0. : std::clamp((scalarValue(v) - tl) / (th - tl), 0., 1.);
    }
  return h == l ? 0. : std::clamp((scalarValue(v) - l) / (h - l), 0., 1.);
}

template <typename T>
inline constexpr bool is_value_tuple = false;
template <typename... T>
inline constexpr bool is_value_tuple<std::tuple<T...>> = true;

//! halp::custom_multi_control. Values are normalized; a gesture is one undo step.
//!
//! Or, when the item's `values` is a std::tuple, each value is the control's
//! own (a list, a 2D point...), kept up to date, and edited through
//! `transactions`, a tuple of halp::transaction, one per control. It may be
//! shorter: the controls past its end are only shown.
template <typename Item>
class CustomMultiControl
    : public QObject
    , public CustomItem<Item>
{
public:
  //! \p edits: where the ui's edits go, so that its gestures are one undo step
  //! each, and come before any other command
  CustomMultiControl(
      Item item_init, std::vector<Process::ControlInlet*> ports, UiEdits& edits)
      : CustomItem<Item>{item_init}
      , m_ports{std::move(ports)}
      , m_edits{edits}
  {
    using values_type = std::decay_t<decltype(this->impl.values)>;
    if constexpr(is_value_tuple<values_type>)
    {
      setupRaw(std::make_index_sequence<std::tuple_size_v<values_type>>{});
    }
    else
    {
      setupNormalized();
    }
  }

  ~CustomMultiControl()
  {
    // Destroyed mid-gesture (UI rebuilt on inlet change): what it did stays
    if(m_active)
      m_edits.endGesture();
  }

private:
  template <std::size_t... I>
  void setupRaw(std::index_sequence<I...>)
  {
    (setupRawPort<I>(), ...);
  }

  template <std::size_t I>
  void setupRawPort()
  {
    using transactions_type = std::decay_t<decltype(this->impl.transactions)>;
    if(I >= m_ports.size())
    {
      // No control to edit: the item's calls do nothing
      if constexpr(I < std::tuple_size_v<transactions_type>)
      {
        auto& tr = std::get<I>(this->impl.transactions);
        tr.start = [] { };
        tr.update = [](const auto&) { };
        tr.commit = [] { };
        tr.rollback = [] { };
      }
      return;
    }
    auto* port = m_ports[I];
    using value_type = std::decay_t<decltype(std::get<I>(this->impl.values))>;
    auto show = [this](const ossia::value& v) {
      value_type val{};
      oscr::from_ossia_value(v, val);
      std::get<I>(this->impl.values) = std::move(val);
      this->update();
    };
    show(port->value());
    QObject::connect(port, &Process::ControlInlet::valueChanged, this, show);

    if constexpr(I < std::tuple_size_v<transactions_type>)
      setupRawTransaction<I>(port);
  }

  template <std::size_t I>
  void setupRawTransaction(Process::ControlInlet* port)
  {
    using value_type = std::decay_t<decltype(std::get<I>(this->impl.values))>;
    auto& tr = std::get<I>(this->impl.transactions);
    tr.start = [this] { start(); };
    tr.update = [this, port](const value_type& val) {
      std::get<I>(this->impl.values) = val;
      m_edits.set(*port, oscr::to_ossia_value(val));
      this->update();
    };
    tr.commit = [this] { commit(); };
    tr.rollback = [this] { rollback(); };
  }

  void setupNormalized()
  {
    const std::size_t n = std::min(m_ports.size(), std::size(this->impl.values));
    for(std::size_t i = 0; i < n; i++)
    {
      auto* port = m_ports[i];
      auto show = [this, i, port](const ossia::value& v) {
        const auto n = normalize(*port, v);
        if(n == this->impl.values[i])
          return;
        this->impl.values[i] = n;
        this->update();
      };
      show(port->value());
      QObject::connect(port, &Process::ControlInlet::valueChanged, this, show);
    }

    this->impl.transaction.start = [this] { start(); };
    this->impl.transaction.update = [this, n](int i, double normalized) {
      if(i < 0 || std::size_t(i) >= n)
        return;
      auto& port = *m_ports[i];
      m_edits.set(port, denormalize(port, normalized));
    };
    this->impl.transaction.commit = [this] { commit(); };
    this->impl.transaction.rollback = [this] { rollback(); };
  }

  //! One gesture of the item, whichever of its transactions it goes through
  void start()
  {
    if(!m_active)
      m_edits.beginGesture();
    m_active = true;
  }
  void commit()
  {
    if(m_active)
      m_edits.endGesture();
    m_active = false;
  }
  void rollback()
  {
    if(m_active)
      m_edits.rollback();
    m_active = false;
  }

  static std::pair<double, double> range(const Process::ControlInlet& port)
  {
    const auto& dom = port.domain().get();
    const auto lo = ossia::get_min(dom);
    const auto hi = ossia::get_max(dom);
    if(!lo.valid() || !hi.valid())
      return {0., 1.};
    // A time chooser's domain is {min, 0} .. {max, 1}
    if(auto a = lo.target<ossia::vec2f>())
      if(auto b = hi.target<ossia::vec2f>())
        return {(*a)[0], (*b)[0]};
    return {ossia::convert<double>(lo), ossia::convert<double>(hi)};
  }

  static double normalize(const Process::ControlInlet& port, const ossia::value& v)
  {
    return normalizedValue(port, v);
  }

  static ossia::value denormalize(const Process::ControlInlet& port, double n)
  {
    const auto [lo, hi] = range(port);
    const double v = lo + std::clamp(n, 0., 1.) * (hi - lo);
    // Keep the control's own type
    if(port.value().get_type() == ossia::val_type::INT)
      return int(std::lround(v));
    // A time chooser: dragged on a graph, a length in seconds (a synced
    // stage becomes free there)
    if(port.value().get_type() == ossia::val_type::VEC2F)
      return ossia::vec2f{float(v), 0.f};
    return float(v);
  }

  std::vector<Process::ControlInlet*> m_ports;
  UiEdits& m_edits;
  bool m_active{};
};
}
