#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::PortItem)

namespace Dataflow
{
void onCreateCable(
    const score::DocumentContext& ctx, Dataflow::PortItem* p1, Dataflow::PortItem* p2);

namespace
{
static std::optional<Process::PortType> portDragType{};
enum PortDragDirection
{
  DragSourceIsInlet,
  DragSourceIsOutlet
} portDragDirection{};
static QLineF portDragLineCoords{};
static PortItem* magneticDropPort{};
struct Ellipses
{
  std::array<QPixmap, 5> SmallEllipsesIn;
  std::array<QPixmap, 5> LargeEllipsesIn;

  std::array<QPixmap, 5> SmallEllipsesOut;
  std::array<QPixmap, 5> LargeEllipsesOut;

  std::array<QPixmap, 5> SmallEllipsesInLight;
  std::array<QPixmap, 5> LargeEllipsesInLight;

  std::array<QPixmap, 5> SmallEllipsesOutLight;
  std::array<QPixmap, 5> LargeEllipsesOutLight;

  Ellipses()
  {
    auto& skin = Process::Style::instance();
    static constexpr qreal smallRadius = 3.;
    static constexpr qreal largeRadius = 5.;
    static constexpr QRectF smallEllipse{3., 3., 2. * smallRadius, 2. * smallRadius};
    static constexpr QRectF largeEllipse{1., 1., 2. * largeRadius, 2. * largeRadius};
    const qreal dpi = qApp->devicePixelRatio();
    const qreal sz = dpi * 13.;

#define DRAW_ELLIPSE(Image, Pen, Brush, Ellipse)              \
  do                                                          \
  {                                                           \
    QImage temp(sz, sz, QImage::Format_ARGB32_Premultiplied); \
    temp.fill(Qt::transparent);                               \
    temp.setDevicePixelRatio(dpi);                            \
    QPainter p(&temp);                                        \
    p.setRenderHint(QPainter::Antialiasing, true);            \
    p.setPen(Pen);                                            \
    p.setBrush(Brush);                                        \
    p.drawEllipse(Ellipse);                                   \
    Image = QPixmap::fromImage(temp);                         \
  } while(0)

    const auto& audiopen = skin.AudioPortPen();
    const auto& datapen = skin.DataPortPen();
    const auto& midipen = skin.MidiPortPen();
    const auto& texturepen = skin.skin.LightGray.main.pen1_5;
    const auto& geometrypen = skin.skin.Emphasis3.main.pen1_5;
    const auto& audiopen_light = skin.skin.Port1.lighter180.pen1_5;
    const auto& datapen_light = skin.skin.Port2.lighter180.pen1_5;
    const auto& midipen_light = skin.skin.Port3.lighter180.pen1_5;
    const auto& texturepen_light = skin.skin.LightGray.lighter180.pen1_5;
    const auto& geometrypen_light = skin.skin.Emphasis3.lighter180.pen1_5;
    const auto& audiobrush = skin.skin.Port1.main.brush;
    const auto& databrush = skin.skin.Port2.main.brush;
    const auto& midibrush = skin.skin.Port3.main.brush;
    const auto& texturebrush = skin.skin.Light.main.brush;
    const auto& geometrybrush = skin.skin.Emphasis3.main.brush;
    const auto& audiobrush_light = skin.skin.Port1.lighter.brush;
    const auto& databrush_light = skin.skin.Port2.lighter.brush;
    const auto& midibrush_light = skin.skin.Port3.lighter.brush;
    const auto& texturebrush_light = skin.skin.LightGray.lighter.brush;
    const auto& geometrybrush_light = skin.skin.Emphasis3.lighter.brush;
    const auto& nobrush = skin.NoBrush();
    DRAW_ELLIPSE(SmallEllipsesIn[0], audiopen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[1], datapen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[2], midipen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[3], texturepen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[4], geometrypen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[0], audiopen, audiobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[1], datapen, databrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[2], midipen, midibrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[3], texturepen, texturebrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[4], geometrypen, geometrybrush, smallEllipse);

    DRAW_ELLIPSE(LargeEllipsesIn[0], audiopen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[1], datapen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[2], midipen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[3], texturepen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[4], geometrypen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[0], audiopen, audiobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[1], datapen, databrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[2], midipen, midibrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[3], texturepen, texturebrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[4], geometrypen, geometrybrush, largeEllipse);

    DRAW_ELLIPSE(SmallEllipsesInLight[0], audiopen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[1], datapen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[2], midipen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[3], texturepen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[4], geometrypen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(
        SmallEllipsesOutLight[0], audiopen_light, audiobrush_light, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOutLight[1], datapen_light, databrush_light, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOutLight[2], midipen_light, midibrush_light, smallEllipse);
    DRAW_ELLIPSE(
        SmallEllipsesOutLight[3], texturepen_light, texturebrush_light, smallEllipse);
    DRAW_ELLIPSE(
        SmallEllipsesOutLight[4], geometrypen_light, geometrybrush_light, smallEllipse);

    DRAW_ELLIPSE(LargeEllipsesInLight[0], audiopen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[1], datapen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[2], midipen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[3], texturepen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[4], geometrypen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(
        LargeEllipsesOutLight[0], audiopen_light, audiobrush_light, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOutLight[1], datapen_light, databrush_light, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOutLight[2], midipen_light, midibrush_light, largeEllipse);
    DRAW_ELLIPSE(
        LargeEllipsesOutLight[3], texturepen_light, texturebrush_light, largeEllipse);
    DRAW_ELLIPSE(
        LargeEllipsesOutLight[4], geometrypen_light, geometrybrush_light, largeEllipse);

#undef DRAW_ELLIPSE
  }
};
}
const QPixmap&
PortItem::portImage(Process::PortType t, bool inlet, bool small, bool light) noexcept
{
  static const Ellipses ellipses;
  int n;
  switch(t)
  {
    case Process::PortType::Audio:
      n = 0;
      break;
    case Process::PortType::Message:
      n = 1;
      break;
    case Process::PortType::Midi:
      n = 2;
      break;
    case Process::PortType::Texture:
      n = 3;
      break;
    case Process::PortType::Geometry:
      n = 4;
      break;
    default:
      n = 1;
      break;
  };

  if(Q_UNLIKELY(light))
  {
    if(inlet)
    {
      if(small)
      {
        return ellipses.SmallEllipsesInLight[n];
      }
      else
      {
        return ellipses.LargeEllipsesInLight[n];
      }
    }
    else
    {
      if(small)
      {
        return ellipses.SmallEllipsesOutLight[n];
      }
      else
      {
        return ellipses.LargeEllipsesOutLight[n];
      }
    }
  }
  else
  {
    if(inlet)
    {
      if(small)
      {
        return ellipses.SmallEllipsesIn[n];
      }
      else
      {
        return ellipses.LargeEllipsesIn[n];
      }
    }
    else
    {
      if(small)
      {
        return ellipses.SmallEllipsesOut[n];
      }
      else
      {
        return ellipses.LargeEllipsesOut[n];
      }
    }
  }
}

PortItem* PortItem::clickedPort;
PortItem::PortItem(
    const Process::Port& p, const Process::Context& ctx, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_context{ctx}
    , m_port{p}
    , m_diam{8.}
    , m_inlet{bool(qobject_cast<const Process::Inlet*>(&p))}
    , m_highlight{false}
{
  auto& skin = score::Skin::instance();
  this->setCursor(skin.CursorPointingHand);
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
  this->setFlag(QGraphicsItem::ItemIsFocusable);
  if(!p.description().isEmpty())
    this->setToolTip(p.description());
  else if(!p.exposed().isEmpty())
    this->setToolTip(p.exposed());

  connect(&p, &QObject::destroyed, this, [] {
    qDebug("Port destroyed before its item");
    SCORE_ASSERT(false);
  });

  con(p.selection, &Selectable::changed, this, [this](bool b) {
    if(b)
      this->setFocus(Qt::OtherFocusReason);
    else
      this->clearFocus();

    for(const auto& cable : cables)
    {
      cable->setZValue(b || cable->model().selection.get() ? 999999 : -1);
    }
  });
  {
    auto& plug = ctx.dataflow;
    // Note : ports can exist multiple times due to the loop thing.
    // If a port already exists it becomes zombie-like.

    auto it = plug.ports().find(&p);
    if(it != plug.ports().end())
    {
      setVisible(false);
      setEnabled(false);
      return;
    }

    plug.ports().insert({&p, this});

    Path<Process::Port> path = p;
    for(auto c : plug.cables())
    {
      if(c.first->source().unsafePath() == path.unsafePath())
      {
        if(c.second)
        {
          c.second->setSource(this);
          cables.push_back(c.second);
        }
        else
        {
          m_context.dataflow.createCable(*c.first, m_context, scene());
        }
      }
      else if(c.first->sink().unsafePath() == path.unsafePath())
      {
        if(c.second)
        {
          c.second->setTarget(this);
          cables.push_back(c.second);
        }
        else
        {
          m_context.dataflow.createCable(*c.first, m_context, scene());
        }
      }
    }

    QObject::connect(
        this, &Dataflow::PortItem::contextMenuRequested, this,
        [&](QPointF sp, QPoint p) {
      auto menu = new QMenu{};
      setupMenu(*menu, ctx);
      menu->exec(p);
      menu->deleteLater();
        });
  }
  setVisible(CableItem::g_cables_enabled);
}

PortItem::~PortItem()
{
  if(this == magneticDropPort)
    magneticDropPort = nullptr;

  for(const auto& cable : cables)
  {
    if(cable->source() == this)
      cable->setSource(nullptr);
    if(cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto& plug = m_context.dataflow;
  auto& p = plug.ports();
  auto it = p.find(&m_port);
  if(it != p.end())
    p.erase(it);
}

void PortItem::setupMenu(QMenu&, const score::DocumentContext& ctx) { }

void PortItem::setPortVisible(bool b)
{
  m_visible = b;
  resetPortVisible();
}
void PortItem::resetPortVisible()
{
  bool v = m_visible && CableItem::g_cables_enabled;
  if(v != isVisible())
    setVisible(v);
}

void PortItem::setHighlight(bool b)
{
  m_highlight = b;
  update();
}

QRectF PortItem::boundingRect() const
{
  constexpr auto max_diam = 13.;
  return {0., 0., max_diam, max_diam};
}
void PortItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const QPixmap& img = portImage(m_port.type(), m_inlet, m_diam == 8., m_highlight);
  painter->drawPixmap(0, 0, img);
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
  {
    switch(event->button())
    {
      case Qt::RightButton:
        contextMenuRequested(event->scenePos(), event->screenPos());
        break;
      case Qt::LeftButton:
      default:
        break;
    }
  }
  event->accept();
}

static void updateDragLineCoords(QGraphicsScene& scene, QPointF pt)
{
  if(magneticDropPort)
  {
    magneticDropPort->m_diam = 8;
    magneticDropPort->update();
  }

  if(portDragType)
  {
    auto items = scene.items({pt.x() - 8, pt.y() - 8, 16, 16});
    PortItem** closePorts = (PortItem**)alloca(items.size() * sizeof(PortItem*));
    int count = 0;
    for(auto item : items)
    {
      if(item->type() == QGraphicsItem::UserType + 700)
      {
        auto port = safe_cast<PortItem*>(item);
        if(portDragDirection == DragSourceIsInlet && !port->m_inlet)
        {
          if(portDragType == port->port().type())
          {
            closePorts[count++] = port;
          }
        }
      }
    }

    if(count > 0)
    {
      QPointF cur_center{};
      double cur_length = 1000.;
      PortItem* cur_port{};
      for(int i = 0; i < count; i++)
      {
        auto port = closePorts[i];
        auto port_center
            = port->mapToScene(((QGraphicsItem*)port)->boundingRect().center());
        if(double length = QLineF{port_center, pt}.length(); length < cur_length)
        {
          cur_length = length;
          cur_port = port;
          cur_center = port_center;
        }
      }

      portDragLineCoords.setP2(cur_center);
      magneticDropPort = cur_port;
      if(magneticDropPort)
      {
        magneticDropPort->m_diam = 12;
        magneticDropPort->update();
      }
      return;
    }
  }
  magneticDropPort = nullptr;
  portDragLineCoords.setP2(pt);
}
struct DragLine : QGraphicsLineItem
{
public:
  DragLine(QLineF f)
      : QGraphicsLineItem{f}
  {
    setPen(QPen(
        QBrush{qRgb(200, 200, 210)}, 2, Qt::PenStyle::SolidLine,
        Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin));

    setAcceptHoverEvents(true);
  }
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    updateDragLineCoords(*scene(), event->scenePos());
    setLine(portDragLineCoords);
  }
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
  {
    updateDragLineCoords(*scene(), event->scenePos());
    setLine(portDragLineCoords);
  }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    updateDragLineCoords(*scene(), event->scenePos());
    setLine(portDragLineCoords);
  }

  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget = nullptr) override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    QGraphicsLineItem::paint(painter, option, widget);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
};

static QGraphicsLineItem* portDragLine{};
struct DragMoveFilter : QObject
{
public:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if(event->type() == QEvent::GraphicsSceneDragMove)
    {
      auto ev = static_cast<QGraphicsSceneDragDropEvent*>(event);
      updateDragLineCoords(*portDragLine->scene(), ev->scenePos());
      portDragLine->setLine(portDragLineCoords);
      return false;
    }
    else if(event->type() == QEvent::GraphicsSceneDrop)
    {
      auto ev = static_cast<QGraphicsSceneDragDropEvent*>(event);
      updateDragLineCoords(*portDragLine->scene(), ev->scenePos());

      if(magneticDropPort)
      {
        magneticDropPort->dropEvent(ev);
        magneticDropPort->m_diam = 8;
        magneticDropPort->update();
        portDragType = std::nullopt;
        return true;
      }
    }

    return QObject::eventFilter(watched, event);
  }
};

static DragMoveFilter* drag_move_filter{};
void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QPointer<QDrag> d{new QDrag{this}};
    QMimeData* m = new QMimeData;
    portDragType = this->m_port.type();
    portDragDirection = this->m_inlet ? DragSourceIsInlet : DragSourceIsOutlet;
    portDragLineCoords
        = QLineF{scenePos() + QPointF{6., 6.}, event->scenePos() + QPointF{6., 6.}};
    portDragLine = new DragLine{portDragLineCoords};

    scene()->installEventFilter(drag_move_filter = new DragMoveFilter{});
    scene()->addItem(portDragLine);
    clickedPort = this;
    m->setData(score::mime::port(), {});
    d->setMimeData(m);

    // NOTE ! from this point, one mustn't ever ever access any member from our PortItem.
    // This is because some drag actions may remove the port, by
    // e.g. moving the process somewhere else or something like that
    // Thus we put stuff in a lambda to make sure that we only access what is necessary
    [d, sc = this->scene(), self = this] {
      connect(d, &QDrag::destroyed, self, [sc] {
        sc->removeEventFilter(drag_move_filter);
        clickedPort = nullptr;
        delete portDragLine;
        portDragLine = nullptr;
        delete drag_move_filter;
        drag_move_filter = nullptr;
      });

      d->exec();
      if(d)
      {
        delete d;
      }
      else
      {
        if(drag_move_filter)
        {
          sc->removeEventFilter(drag_move_filter);
          clickedPort = nullptr;
          delete portDragLine;
          portDragLine = nullptr;
          delete drag_move_filter;
          drag_move_filter = nullptr;
        }
      }
        }();
  }
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
  {
    event->accept();
    switch(event->button())
    {
      case Qt::LeftButton: {
        score::SelectionDispatcher disp{m_context.selectionStack};
        disp.select(m_port);
        break;
      }
      default:
        break;
    }
  }
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  event->accept();
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  if(portDragType && *portDragType == this->port().type())
  {
    if(portDragDirection == DragSourceIsInlet && !this->m_inlet)
    {
      prepareGeometryChange();
      m_diam = 12.;
      update();
    }
  }
  event->accept();
}

void PortItem::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void PortItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

ossia::small_vector<const Process::Port*, 16>
getProcessPorts(const Process::ProcessModel& proc)
{
  ossia::small_vector<const Process::Port*, 16> ports;
  auto& inlets = proc.inlets();
  auto& outlets = proc.outlets();
  ports.reserve(inlets.size() + outlets.size());
  ports.insert(ports.end(), inlets.begin(), inlets.end());
  ports.insert(ports.end(), outlets.begin(), outlets.end());
  return ports;
}

void PortItem::keyPressEvent(QKeyEvent* event)
{
  enum
  {
    Nothing,
    SelectCable,
    SelectPreviousPort,
    SelectNextPort
  } action{};

  switch(event->key())
  {
    // It's an inlet: cable is at the left
    case Qt::Key_Left:
      if(qobject_cast<const Process::Inlet*>(&m_port))
        action = SelectCable;
      break;
    case Qt::Key_Right:
      if(qobject_cast<const Process::Outlet*>(&m_port))
        action = SelectCable;
      break;
    case Qt::Key_Up:
      action = SelectPreviousPort;
      break;
    case Qt::Key_Down:
      action = SelectNextPort;
      break;
  }

  score::SelectionDispatcher disp{m_context.selectionStack};
  switch(action)
  {
    case Nothing:
      break;
    case SelectCable:
      if(!this->cables.empty())
        disp.select(this->cables.front()->model());
      break;
    case SelectPreviousPort: {
      if(auto proc = qobject_cast<Process::ProcessModel*>(m_port.parent()))
      {
        auto ports = getProcessPorts(*proc);
        auto it = ossia::find(ports, &m_port);
        SCORE_ASSERT(it != ports.end());
        if(it == ports.begin())
        {
          disp.select(*proc);
        }
        else
        {
          std::advance(it, -1);
          disp.select(**it);
        }
      }
      break;
    }
    case SelectNextPort: {
      if(auto proc = qobject_cast<Process::ProcessModel*>(m_port.parent()))
      {
        auto ports = getProcessPorts(*proc);
        auto it = ossia::find(ports, &m_port);
        SCORE_ASSERT(it != ports.end());
        std::advance(it, 1);
        if(it != ports.end())
          disp.select(**it);
      }
      break;
    }
  }

  event->accept();
}

void PortItem::keyReleaseEvent(QKeyEvent* event)
{
  event->accept();
}

QVariant
PortItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch(change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      for(auto cbl : cables)
      {
        if(cbl)
          cbl->check();
      }
      break;
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for(auto cbl : cables)
      {
        if(cbl)
          cbl->check();
      }
      break;
    default:
      break;
  }

  return QGraphicsItem::itemChange(change, value);
}

score::SimpleTextItem* makePortLabel(const Process::Port& port, QGraphicsItem* parent)
{
  const auto& brush = Process::labelBrush(port);

  auto lab = new score::SimpleTextItem{brush, parent};
  lab->setText(port.visualName());

  QObject::connect(&port.selection, &Selectable::changed, lab, [lab, &port](bool b) {
    lab->setColor(Process::labelBrush(port));
  });

  QObject::connect(
      &port, &Process::Port::nameChanged, lab,
      qOverload<const QString&>(&score::SimpleTextItem::setText));

  return lab;
}

}

namespace Process
{
const score::Brush& portBrush(Process::PortType type)
{
  const auto& skin = score::Skin::instance();
  switch(type)
  {
    case Process::PortType::Audio:
      return skin.Port1;
    case Process::PortType::Message:
      return skin.Port2;
    case Process::PortType::Midi:
      return skin.Port3;
    case Process::PortType::Texture:
      return skin.LightGray;
    case Process::PortType::Geometry:
      return skin.Emphasis3;
    default:
      return skin.Warn1;
  }
}
const score::Brush& labelBrush()
{
  return score::Skin::instance().HalfLight;
}
const score::BrushSet& labelBrush(const Process::Port& p)
{
  if(!p.selection.get())
    return score::Skin::instance().HalfLight.main;
  else
    return score::Skin::instance().Base2.main;
}

}
