#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QGraphicsObject>
#include <QPointer>
#include <QTimer>

#include <score_lib_process_export.h>

namespace Process
{
class Port;
struct Context;

/**
 * A live view of what goes through a port, opened when the cursor rests on
 * the port or on one of its cables; it closes when the cursor leaves both
 * the port and the probe.
 */
class SCORE_LIB_PROCESS_EXPORT PortProbeItem : public QGraphicsObject
{
public:
  using QGraphicsObject::QGraphicsObject;
  ~PortProbeItem() override;
};

class SCORE_LIB_PROCESS_EXPORT PortProbeFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(PortProbeFactory, "6c2f1d8e-3a45-4b9e-8f7c-2d5a1e9b0c43")
public:
  ~PortProbeFactory() override;
  //! A probe for `port`, or null when there is nothing to show.
  virtual PortProbeItem* make(const Process::Port& port, const Process::Context& ctx) = 0;
};

class SCORE_LIB_PROCESS_EXPORT PortProbeFactoryList final
    : public score::InterfaceList<PortProbeFactory>
{
public:
  ~PortProbeFactoryList() override;
};

/**
 * The one probe the cursor opens: after it rests on a port, until it leaves
 * the port and the probe, or the port, or the item it rested on, is gone.
 */
class SCORE_LIB_PROCESS_EXPORT PortProbeHover final : public QObject
{
public:
  //! Never destroyed: its timers must not outlive the application object
  static PortProbeHover& instance();

  void enter(const Process::Port& port, const Process::Context& ctx, QGraphicsScene& scene, QPointF scenePos);
  void leave();
  //! The cursor is over the probe: keep it.
  void hold() { m_closeTimer.stop(); }

private:
  PortProbeHover();
  void open();
  void close();
  void closeUnlessHeld();
  //! A deleted item gets no hover leave: while a probe is open, it closes
  //! once the cursor is neither over it nor over a port or a cable.
  void checkHovered();

  QPointer<const Process::Port> m_port;
  QMetaObject::Connection m_portDestroyed;
  const Process::Context* m_context{};
  QPointer<QGraphicsScene> m_scene;
  QPointF m_pos;
  QPointer<PortProbeItem> m_item;
  QTimer m_openTimer;
  QTimer m_closeTimer;
  QTimer m_checkTimer;
  int m_missed{};
};
}
