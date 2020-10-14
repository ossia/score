#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>

#include <tsl/hopscotch_map.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::PortItem)

namespace Dataflow
{
void onCreateCable(
    const score::DocumentContext& ctx,
    Dataflow::PortItem* p1,
    Dataflow::PortItem* p2);

namespace
{
static QLineF portDragLineCoords{};
static PortItem* magneticDropPort{};
struct Ellipses
{
  std::array<QPixmap, 4> SmallEllipsesIn;
  std::array<QPixmap, 4> LargeEllipsesIn;

  std::array<QPixmap, 4> SmallEllipsesOut;
  std::array<QPixmap, 4> LargeEllipsesOut;

  std::array<QPixmap, 4> SmallEllipsesInLight;
  std::array<QPixmap, 4> LargeEllipsesInLight;

  std::array<QPixmap, 4> SmallEllipsesOutLight;
  std::array<QPixmap, 4> LargeEllipsesOutLight;

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
  } while (0)

    const auto& audiopen = skin.AudioPortPen();
    const auto& datapen = skin.DataPortPen();
    const auto& midipen = skin.MidiPortPen();
    const auto& texturepen = skin.skin.LightGray.main.pen1_5;
    const auto& audiopen_light = skin.skin.Port1.lighter180.pen1_5;
    const auto& datapen_light = skin.skin.Port2.lighter180.pen1_5;
    const auto& midipen_light = skin.skin.Port3.lighter180.pen1_5;
    const auto& texturepen_light = skin.skin.LightGray.lighter180.pen1_5;
    const auto& audiobrush = skin.skin.Port1.main.brush;
    const auto& databrush = skin.skin.Port2.main.brush;
    const auto& midibrush = skin.skin.Port3.main.brush;
    const auto& texturebrush = skin.skin.Light.main.brush;
    const auto& audiobrush_light = skin.skin.Port1.lighter.brush;
    const auto& databrush_light = skin.skin.Port2.lighter.brush;
    const auto& midibrush_light = skin.skin.Port3.lighter.brush;
    const auto& texturebrush_light = skin.skin.LightGray.lighter.brush;
    const auto& nobrush = skin.NoBrush();
    DRAW_ELLIPSE(SmallEllipsesIn[0], audiopen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[1], datapen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[2], midipen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesIn[3], texturepen, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[0], audiopen, audiobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[1], datapen, databrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[2], midipen, midibrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOut[3], texturepen, texturebrush, smallEllipse);

    DRAW_ELLIPSE(LargeEllipsesIn[0], audiopen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[1], datapen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[2], midipen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesIn[3], texturepen, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[0], audiopen, audiobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[1], datapen, databrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[2], midipen, midibrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOut[3], texturepen, texturebrush, largeEllipse);

    DRAW_ELLIPSE(SmallEllipsesInLight[0], audiopen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[1], datapen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[2], midipen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesInLight[3], texturepen_light, nobrush, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOutLight[0], audiopen_light, audiobrush_light, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOutLight[1], datapen_light, databrush_light, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOutLight[2], midipen_light, midibrush_light, smallEllipse);
    DRAW_ELLIPSE(SmallEllipsesOutLight[3], texturepen_light, texturebrush_light, smallEllipse);

    DRAW_ELLIPSE(LargeEllipsesInLight[0], audiopen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[1], datapen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[2], midipen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesInLight[3], texturepen_light, nobrush, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOutLight[0], audiopen_light, audiobrush_light, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOutLight[1], datapen_light, databrush_light, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOutLight[2], midipen_light, midibrush_light, largeEllipse);
    DRAW_ELLIPSE(LargeEllipsesOutLight[3], texturepen_light, texturebrush_light, largeEllipse);

#undef DRAW_ELLIPSE
  }
};
}
const QPixmap&
PortItem::portImage(Process::PortType t, bool inlet, bool small, bool light) noexcept
{
  static const Ellipses ellipses;
  int n;
  switch (t)
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
    default:
      n = 1;
      break;
  };

  if (Q_UNLIKELY(light))
  {
    if (inlet)
    {
      if (small)
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
      if (small)
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
    if (inlet)
    {
      if (small)
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
      if (small)
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
PortItem::PortItem(const Process::Port& p, const Process::Context& ctx, QGraphicsItem* parent)
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
  this->setToolTip(p.customData());

  connect(&p, &QObject::destroyed, this, [] {
    qDebug("Port destroyed before its item");
    SCORE_ASSERT(false);
  });

  con(p.selection, &Selectable::changed, this, [this](bool b) {
    for (const auto& cable : cables)
    {
      cable->setZValue(b || cable->model().selection.get() ? 999999 : -1);
    }
  });
  {
    auto& plug = ctx.dataflow;
    // Note : ports can exist multiple times due to the loop thing.
    // If a port already exists it becomes zombie-like.

    auto it = plug.ports().find(&p);
    if (it != plug.ports().end())
    {
      setVisible(false);
      setEnabled(false);
      return;
    }

    plug.ports().insert({&p, this});

    Path<Process::Port> path = p;
    for (auto c : plug.cables())
    {
      if (c.first->source().unsafePath() == path.unsafePath())
      {
        if (c.second)
        {
          c.second->setSource(this);
          cables.push_back(c.second);
        }
        else
        {
          m_context.dataflow.createCable(*c.first, m_context, scene());
        }
      }
      else if (c.first->sink().unsafePath() == path.unsafePath())
      {
        if (c.second)
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
        this, &Dataflow::PortItem::contextMenuRequested, this, [&](QPointF sp, QPoint p) {
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
  if (this == magneticDropPort)
    magneticDropPort = nullptr;

  for (auto cable : cables)
  {
    if (cable->source() == this)
      cable->setSource(nullptr);
    if (cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto& plug = m_context.dataflow;
  auto& p = plug.ports();
  auto it = p.find(&m_port);
  if (it != p.end())
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
  if (v != isVisible())
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
void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const QPixmap& img = portImage(m_port.type(), m_inlet, m_diam == 8., m_highlight);
  painter->drawPixmap(0, 0, img);
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (this->contains(event->pos()))
  {
    switch (event->button())
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
  if (magneticDropPort)
  {
    magneticDropPort->m_diam = 8;
    magneticDropPort->update();
  }

  auto items = scene.items({pt.x() - 8, pt.y() - 8, 16, 16});
  PortItem** closePorts = (PortItem**)alloca(items.size() * sizeof(PortItem*));
  int count = 0;
  for (auto item : items)
  {
    if (item->type() == QGraphicsItem::UserType + 700)
    {
      closePorts[count++] = safe_cast<PortItem*>(item);
    }
  }

  if (count > 0)
  {
    QPointF cur_center{};
    double cur_length = 1000.;
    PortItem* cur_port{};
    for (int i = 0; i < count; i++)
    {
      auto port = closePorts[i];
      auto port_center = port->mapToScene(((QGraphicsItem*)port)->boundingRect().center());
      if (double length = QLineF{port_center, pt}.length(); length < cur_length)
      {
        cur_length = length;
        cur_port = port;
        cur_center = port_center;
      }
    }

    portDragLineCoords.setP2(cur_center);
    magneticDropPort = cur_port;
    if (magneticDropPort)
    {
      magneticDropPort->m_diam = 12;
      magneticDropPort->update();
    }
    return;
  }
  magneticDropPort = nullptr;
  portDragLineCoords.setP2(pt);
}
struct DragLine : QGraphicsLineItem
{
public:
  DragLine(QLineF f) : QGraphicsLineItem{f}
  {
    setPen(QPen(
        QBrush{qRgb(200, 200, 210)},
        2,
        Qt::PenStyle::SolidLine,
        Qt::PenCapStyle::RoundCap,
        Qt::PenJoinStyle::RoundJoin));

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

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr)
      override
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
    if (event->type() == QEvent::GraphicsSceneDragMove)
    {
      auto ev = static_cast<QGraphicsSceneDragDropEvent*>(event);
      updateDragLineCoords(*portDragLine->scene(), ev->scenePos());
      portDragLine->setLine(portDragLineCoords);
      return false;
    }
    else if (event->type() == QEvent::GraphicsSceneDrop)
    {
      auto ev = static_cast<QGraphicsSceneDragDropEvent*>(event);
      updateDragLineCoords(*portDragLine->scene(), ev->scenePos());

      if (magneticDropPort)
      {
        magneticDropPort->dropEvent(ev);
        magneticDropPort->m_diam = 8;
        magneticDropPort->update();
      }
      return true;
    }
    else
    {
      return QObject::eventFilter(watched, event);
    }
  }
};

static DragMoveFilter* drag_move_filter{};
void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if (QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QDrag* d{new QDrag{this}};
    QMimeData* m = new QMimeData;
    portDragLineCoords = QLineF{scenePos() + QPointF{6., 6.}, event->scenePos() + QPointF{6., 6.}};
    portDragLine = new DragLine{portDragLineCoords};

    scene()->installEventFilter(drag_move_filter = new DragMoveFilter{});
    scene()->addItem(portDragLine);
    clickedPort = this;
    m->setData(score::mime::port(), {});
    d->setMimeData(m);
    d->exec();

    connect(d, &QDrag::destroyed, this, [this] {
      scene()->removeEventFilter(drag_move_filter);
      clickedPort = nullptr;
      delete portDragLine;
      delete drag_move_filter;
    });
  }
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (this->contains(event->pos()))
  {
    event->accept();
    switch (event->button())
    {
      case Qt::LeftButton:
      {
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
  prepareGeometryChange();
  m_diam = 12.;
  update();
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

QVariant PortItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      for (auto cbl : cables)
      {
        if (cbl)
          cbl->resize();
      }
      break;
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for (auto cbl : cables)
      {
        if (cbl)
          cbl->check();
      }
      break;
    default:
      break;
  }

  return QGraphicsItem::itemChange(change, value);
}
}

namespace Process
{
const score::Brush& portBrush(Process::PortType type)
{
  const auto& skin = score::Skin::instance();
  switch (type)
  {
    case Process::PortType::Audio:
      return skin.Port1;
    case Process::PortType::Message:
      return skin.Port2;
    case Process::PortType::Midi:
      return skin.Port3;
    case Process::PortType::Texture:
      return skin.LightGray;
    default:
      return skin.Warn1;
  }
}

}
