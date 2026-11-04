#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <Process/Commands/SetControlValue.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>

#include <QElapsedTimer>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QLineF>
#include <QStyleHints>

#include <Ui/Monitor/ModeSwitch.hpp>

#include <algorithm>

/**
 * What the views of monitoring processes share: reading their settings from
 * the model, and showing the inlet they draw.
 */
namespace Ui::Monitor
{
//! The control inlet at `index`, or null: processes saved before an inlet
//! was added do not have it until they are reloaded.
inline const Process::ControlInlet*
controlInlet(const Process::ProcessModel& p, std::size_t index) noexcept
{
  if(index < p.inlets().size())
    return qobject_cast<const Process::ControlInlet*>(p.inlets()[index]);
  return nullptr;
}

template <typename E>
E enumValue(const Process::ControlInlet* c, E fallback, E last) noexcept
{
  if(!c)
    return fallback;
  return static_cast<E>(std::clamp(ossia::convert<int>(c->value()), 0, int(last)));
}

inline float floatValue(const Process::ControlInlet* c, float fallback) noexcept
{
  return c ? ossia::convert<float>(c->value()) : fallback;
}

inline bool boolValue(const Process::ControlInlet* c, bool fallback) noexcept
{
  return c ? ossia::convert<bool>(c->value()) : fallback;
}

//! Where a fully custom item puts its first inlet: centered on the strip at
//! its top where the mode switch is, as the inlets of a node with a title are
//! on its title.
inline constexpr double firstInletY
    = Process::ModeSwitch::position.y() + Process::ModeSwitch::height / 2.
      - Dataflow::PortItem::portCenter().y();
//! Distance between two inlets shown one under the other
inline constexpr double inletSpacing = 10.;

//! A fully custom item draws no inlets of its own: show the one it
//! monitors at its top-left corner.
inline void showInlet(
    const Process::ProcessModel& p, std::size_t index, const Process::Context& doc,
    QGraphicsItem* parent, QObject* owner, double y = firstInletY)
{
  if(index >= p.inlets().size())
    return;
  auto* inlet = p.inlets()[index];
  const auto& factories = doc.app.interfaces<Process::PortFactoryList>();
  if(auto fact = factories.get(inlet->concreteKey()))
    if(auto port = fact->makePortItem(*inlet, doc, parent, owner))
      port->setPos(0, y);
}

//! Repaints `item` whenever one of the given controls changes in the model.
template <typename Item, typename... C>
void repaintOn(Item* item, C*... controls)
{
  for(const Process::ControlInlet* c : {controls...})
    if(c)
      QObject::connect(
          c, &Process::ControlInlet::valueChanged, item, [item] { item->update(); });
}

/**
 * A double click on a view whose single presses go on to the node, to select
 * or move it. The scene hands the second press of a double click to whoever
 * took the first, the node, so it is recognised here from the presses: call
 * it from mousePressEvent, accept the event when it returns true, ignore it
 * otherwise.
 */
class DoubleClick
{
public:
  bool operator()(const QGraphicsSceneMouseEvent& ev)
  {
    if(ev.button() != Qt::LeftButton)
      return false;
    const bool second
        = m_last.isValid()
          && m_last.elapsed() < QGuiApplication::styleHints()->mouseDoubleClickInterval()
          && QLineF(ev.screenPos(), m_pos).length() < 6.;
    if(second)
    {
      m_last.invalidate();
      return true;
    }
    m_last.restart();
    m_pos = ev.screenPos();
    return false;
  }

private:
  QElapsedTimer m_last;
  QPoint m_pos;
};

//! Sets several controls as one undoable edit.
inline void setControls(
    const score::CommandStackFacade& stack,
    std::initializer_list<std::pair<const Process::ControlInlet*, ossia::value>> values)
{
  MultiOngoingCommandDispatcher disp{stack};
  bool any = false;
  for(const auto& [c, v] : values)
    if(c)
    {
      disp.submit(new Process::SetControlValue{*c, v});
      any = true;
    }
  if(any)
    disp.commit<Process::SetControlValues>();
}
}
