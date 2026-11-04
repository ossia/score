#include "PortProbe.hpp"

#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/PreviewSettings.hpp>
#include <Process/ProcessContext.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace Process
{
PortProbeItem::~PortProbeItem() = default;
PortProbeFactory::~PortProbeFactory() = default;
PortProbeFactoryList::~PortProbeFactoryList() = default;

PortProbeHover& PortProbeHover::instance()
{
  static auto* hover = new PortProbeHover;
  return *hover;
}

PortProbeHover::PortProbeHover()
{
  m_openTimer.setSingleShot(true);
  m_openTimer.setInterval(300);
  connect(&m_openTimer, &QTimer::timeout, this, [this] { open(); });
  m_closeTimer.setSingleShot(true);
  m_closeTimer.setInterval(250);
  connect(&m_closeTimer, &QTimer::timeout, this, [this] { closeUnlessHeld(); });
  connect(
      &Process::PreviewSettings::instance(), &Process::PreviewSettings::hoverProbesChanged,
      this, [this](bool on) {
    if(!on)
    {
      m_openTimer.stop();
      close();
    }
  });
  m_checkTimer.setInterval(500);
  connect(&m_checkTimer, &QTimer::timeout, this, [this] { checkHovered(); });
}

void PortProbeHover::enter(
    const Process::Port& port, const Process::Context& ctx, QGraphicsScene& scene,
    QPointF scenePos)
{
  // Textures are shown by rendering a preview, which has its own switch
  auto& settings = Process::PreviewSettings::instance();
  if(!settings.hoverProbes())
    return;
  if(port.type() == Process::PortType::Texture && !settings.enabled())
    return;
  m_closeTimer.stop();
  if(m_port == &port && m_item)
    return;
  if(m_port != &port)
  {
    disconnect(m_portDestroyed);
    // A probe on a removed port would tap what no longer runs
    m_portDestroyed
        = connect(&port, &QObject::destroyed, this, [this] { close(); });
  }
  m_port = &port;
  m_context = &ctx;
  m_scene = &scene;
  m_pos = scenePos;
  m_openTimer.start();
}

void PortProbeHover::leave()
{
  m_openTimer.stop();
  if(m_item)
    m_closeTimer.start();
}

void PortProbeHover::open()
{
  if(!m_port || !m_scene || !m_context)
    return;
  // Another port: the previous probe goes
  delete m_item.data();
  m_item = nullptr;

  for(auto& factory : m_context->app.interfaces<PortProbeFactoryList>())
  {
    if(auto* item = factory.make(*m_port, *m_context))
    {
      m_scene->addItem(item);
      item->setZValue(1e6);
      item->setPos(m_pos + QPointF{12, 12});
      m_item = item;
      m_missed = 0;
      m_checkTimer.start();
      break;
    }
  }
}

void PortProbeHover::close()
{
  m_openTimer.stop();
  m_closeTimer.stop();
  m_checkTimer.stop();
  m_missed = 0;
  disconnect(m_portDestroyed);
  delete m_item.data();
  m_item = nullptr;
  m_port = nullptr;
}

void PortProbeHover::closeUnlessHeld()
{
  if(!m_item || m_item->isUnderMouse())
    return;
  close();
}

void PortProbeHover::checkHovered()
{
  if(!m_item || !m_scene || !m_port)
  {
    close();
    return;
  }
  // On the way from the port to the probe
  if(m_closeTimer.isActive())
    return;
  for(QGraphicsView* view : m_scene->views())
  {
    const QPoint local = view->viewport()->mapFromGlobal(QCursor::pos());
    if(!view->viewport()->rect().contains(local))
      continue;
    for(QGraphicsItem* item : view->items(local))
    {
      if(item == m_item.data() || m_item->isAncestorOf(item)
         || qgraphicsitem_cast<Dataflow::PortItem*>(item)
         || qgraphicsitem_cast<Dataflow::CableItem*>(item))
      {
        m_missed = 0;
        return;
      }
    }
  }
  // Twice in a row: a cursor that just crosses an empty space keeps it
  if(++m_missed >= 2)
    close();
}
}
