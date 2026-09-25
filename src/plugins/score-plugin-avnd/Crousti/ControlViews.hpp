#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/command/Dispatchers/SendStrategy.hpp>
#include <score/model/Skin.hpp>

#include <ossia/network/domain/domain.hpp>

#include <Crousti/Painter.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QPainter>

#include <algorithm>
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
    m_value = std::clamp(normalized, 0., 1.);
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

//! A control's value, normalized over its range
inline double normalizedValue(const Process::ControlInlet& port, const ossia::value& v)
{
  const auto& dom = port.domain().get();
  const auto lo = ossia::get_min(dom);
  const auto hi = ossia::get_max(dom);
  if(!lo.valid() || !hi.valid())
    return std::clamp(ossia::convert<double>(v), 0., 1.);
  const double l = ossia::convert<double>(lo), h = ossia::convert<double>(hi);
  return h == l ? 0. : std::clamp((ossia::convert<double>(v) - l) / (h - l), 0., 1.);
}

//! halp::custom_multi_control. Values are normalized; a gesture is one undo step.
template <typename Item>
class CustomMultiControl
    : public QObject
    , public CustomItem<Item>
{
public:
  CustomMultiControl(
      Item item_init, std::vector<Process::ControlInlet*> ports,
      const score::DocumentContext& ctx)
      : CustomItem<Item>{item_init}
      , m_ports{std::move(ports)}
      , m_ctx{ctx}
      , m_cmds(m_ports.size(), nullptr)
  {
    const std::size_t n = std::min(m_ports.size(), std::size(this->impl.values));
    for(std::size_t i = 0; i < n; i++)
    {
      auto* port = m_ports[i];
      auto show = [this, i, port](const ossia::value& v) {
        this->impl.values[i] = normalize(*port, v);
        this->update();
      };
      show(port->value());
      QObject::connect(port, &Process::ControlInlet::valueChanged, this, show);
    }

    this->impl.transaction.start = [this] {
      m_ctx.commandStack.disableActions();
      m_active = true;
    };
    this->impl.transaction.update = [this, n](int i, double normalized) {
      if(i < 0 || std::size_t(i) >= n)
        return;
      auto& port = *m_ports[i];
      const ossia::value v = denormalize(port, normalized);
      if(!m_cmds[i])
        m_cmds[i] = new Process::SetControlValue{port, v};
      else
        m_cmds[i]->update(port, v);
      m_cmds[i]->redo(m_ctx);
    };
    this->impl.transaction.commit = [this] {
      // A click that moved nothing leaves no undo step
      if(std::ranges::any_of(m_cmds, [](auto* cmd) { return cmd != nullptr; }))
      {
        auto* all = new Process::SetControlValues;
        for(auto& cmd : m_cmds)
        {
          if(cmd)
            all->addCommand(std::exchange(cmd, nullptr));
        }
        SendStrategy::Quiet::send(m_ctx.commandStack, all);
      }
      m_ctx.commandStack.enableActions();
      m_active = false;
    };
    this->impl.transaction.rollback = [this] { rollback(); };
  }

  ~CustomMultiControl()
  {
    // Destroyed mid-gesture (UI rebuilt on inlet change): roll back
    if(m_active)
      rollback();
  }

private:
  void rollback()
  {
    for(auto it = m_cmds.rbegin(); it != m_cmds.rend(); ++it)
    {
      if(auto cmd = std::exchange(*it, nullptr))
      {
        cmd->undo(m_ctx);
        delete cmd;
      }
    }
    m_ctx.commandStack.enableActions();
    m_active = false;
  }

  static std::pair<double, double> range(const Process::ControlInlet& port)
  {
    const auto& dom = port.domain().get();
    const auto lo = ossia::get_min(dom);
    const auto hi = ossia::get_max(dom);
    if(!lo.valid() || !hi.valid())
      return {0., 1.};
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
    return float(v);
  }

  std::vector<Process::ControlInlet*> m_ports;
  const score::DocumentContext& m_ctx;
  std::vector<Process::SetControlValue*> m_cmds;
  bool m_active{};
};
}
