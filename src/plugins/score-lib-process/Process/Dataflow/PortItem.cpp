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
#include <State/MessageListSerialization.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>

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

struct PortStyle
{
  std::array<QPixmap, 5> SmallEllipsesIn;
  std::array<QPixmap, 5> LargeEllipsesIn;

  std::array<QPixmap, 5> SmallEllipsesOut;
  std::array<QPixmap, 5> LargeEllipsesOut;

  std::array<QPixmap, 5> SmallEllipsesInLight;
  std::array<QPixmap, 5> LargeEllipsesInLight;

  std::array<QPixmap, 5> SmallEllipsesOutLight;
  std::array<QPixmap, 5> LargeEllipsesOutLight;

  std::array<QPixmap, 5> Address_Inlet_NoCable;
  std::array<QPixmap, 5> Address_Inlet_Cable;
  std::array<QPixmap, 5> Address_Outlet_NoCable;
  std::array<QPixmap, 5> Address_Outlet_Cable;

  static constexpr int penSet(Process::PortType t) noexcept
  {
    switch(t)
    {
      case Process::PortType::Audio:
        return 0;
      case Process::PortType::Message:
        return 1;
      case Process::PortType::Midi:
        return 2;
      case Process::PortType::Texture:
        return 3;
      case Process::PortType::Geometry:
        return 4;
      default:
        return 1;
    }
  }

  struct PenSet
  {
    const QPen& pen_base;
    const QPen& pen_cosmetic_nodot;
    const QPen& pen_cosmetic_dot;
    const QPen& pen_light;
    const QBrush& brush_base;
    const QBrush& brush_light;
  };
  std::array<PenSet, 5> pen_sets;

  static std::array<PenSet, 5> makePenSets()
  {
    auto& skin = Process::Style::instance();
    const auto& audiopen = skin.AudioPortPen();
    auto audiocolor = audiopen.color();
    static auto audiopen_cosmetic_nodot = skin.skin.Port1.main.pen_cosmetic;
    static auto audiopen_cosmetic_dot = audiopen_cosmetic_nodot;
    audiocolor.setAlphaF(0.25);
    audiopen_cosmetic_dot.setColor(audiocolor);

    const auto& datapen = skin.DataPortPen();
    auto datacolor = datapen.color();
    static auto datapen_cosmetic_nodot = skin.skin.Port2.main.pen_cosmetic;
    static auto datapen_cosmetic_dot = datapen_cosmetic_nodot;
    datacolor.setAlphaF(0.25);
    datapen_cosmetic_dot.setColor(datacolor);

    const auto& midipen = skin.MidiPortPen();
    auto midicolor = midipen.color();
    static auto midipen_cosmetic_nodot = skin.skin.Port3.main.pen_cosmetic;
    static auto midipen_cosmetic_dot = midipen_cosmetic_nodot;
    midicolor.setAlphaF(0.25);
    midipen_cosmetic_dot.setColor(midicolor);

    const auto& texturepen = skin.TexturePortPen();
    auto texturecolor = texturepen.color();
    static auto texturepen_cosmetic_nodot = skin.skin.LightGray.main.pen_cosmetic;
    static auto texturepen_cosmetic_dot = texturepen_cosmetic_nodot;
    texturecolor.setAlphaF(0.25);
    texturepen_cosmetic_dot.setColor(audiocolor);

    const auto& geometrypen = skin.GeometryPortPen();
    auto geometrycolor = geometrypen.color();
    static auto geometrypen_cosmetic_nodot = skin.skin.Emphasis3.main.pen_cosmetic;
    static auto geometrypen_cosmetic_dot = geometrypen_cosmetic_nodot;
    geometrycolor.setAlphaF(0.25);
    geometrypen_cosmetic_dot.setColor(geometrycolor);

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

    return {
        PenSet{
            audiopen, audiopen_cosmetic_nodot, audiopen_cosmetic_dot, audiopen_light,
            audiobrush, audiobrush_light},
        PenSet{
            datapen, datapen_cosmetic_nodot, datapen_cosmetic_dot, datapen_light,
            databrush, databrush_light},
        PenSet{
            midipen, midipen_cosmetic_nodot, midipen_cosmetic_dot, midipen_light,
            midibrush, midibrush_light},
        PenSet{
            texturepen, texturepen_cosmetic_nodot, texturepen_cosmetic_dot,
            texturepen_light, texturebrush, texturebrush_light},
        PenSet{
            geometrypen, geometrypen_cosmetic_nodot, geometrypen_cosmetic_dot,
            geometrypen_light, geometrybrush, geometrybrush_light},
    };
  }

  PortStyle()
      : pen_sets{makePenSets()}
  {
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

#define DRAW_ELLIPSE_ADDR_IN(Image, Pen, Brush, Ellipse)                                \
  do                                                                                    \
  {                                                                                     \
    QImage temp(sz, sz, QImage::Format_ARGB32_Premultiplied);                           \
    temp.fill(Qt::transparent);                                                         \
    temp.setDevicePixelRatio(dpi);                                                      \
    QPainter p(&temp);                                                                  \
    p.setRenderHint(QPainter::Antialiasing, true);                                      \
    p.setPen(Pen);                                                                      \
    p.setBrush(Brush);                                                                  \
    p.drawEllipse(Ellipse);                                                             \
    p.setCompositionMode(QPainter::CompositionMode_Overlay);                            \
    p.drawLine(QPointF{3. / dpi, 3. / dpi}, QPointF{(sz - 4.) / dpi, (sz - 4.) / dpi}); \
    Image = QPixmap::fromImage(temp);                                                   \
  } while(0)
#define DRAW_ELLIPSE_ADDR_OUT(Image, Pen, Brush, Ellipse)                               \
  do                                                                                    \
  {                                                                                     \
    QImage temp(sz, sz, QImage::Format_ARGB32_Premultiplied);                           \
    temp.fill(Qt::transparent);                                                         \
    temp.setDevicePixelRatio(dpi);                                                      \
    QPainter p(&temp);                                                                  \
    p.setRenderHint(QPainter::Antialiasing, true);                                      \
    p.setPen(Pen);                                                                      \
    p.setBrush(Brush);                                                                  \
    p.drawEllipse(Ellipse);                                                             \
    p.setCompositionMode(QPainter::CompositionMode_ColorDodge);                         \
    p.drawLine(QPointF{3. / dpi, (sz - 4.) / dpi}, QPointF{(sz - 4.) / dpi, 3. / dpi}); \
    Image = QPixmap::fromImage(temp);                                                   \
  } while(0)

#define DRAW_ARROW_ADDR_INLET(Image, Pen, Brush)              \
  do                                                          \
  {                                                           \
    QImage temp(sz, sz, QImage::Format_ARGB32_Premultiplied); \
    temp.fill(Qt::transparent);                               \
    temp.setDevicePixelRatio(dpi);                            \
    QPainter p(&temp);                                        \
    p.setRenderHint(QPainter::Antialiasing, true);            \
    p.setPen(Pen);                                            \
    p.setBrush(Brush);                                        \
    p.drawLine(QPointF{4.5, 9.5}, QPointF{4.5, 2.5});         \
    p.drawLine(QPointF{2.5, 8}, QPointF{3.5, 9});             \
    p.drawLine(QPointF{6.5, 7}, QPointF{5.5, 8});             \
    Image = QPixmap::fromImage(temp);                         \
  } while(0)

#define DRAW_ARROW_ADDR_OUTLET(Image, Pen, Brush)             \
  do                                                          \
  {                                                           \
    QImage temp(sz, sz, QImage::Format_ARGB32_Premultiplied); \
    temp.fill(Qt::transparent);                               \
    temp.setDevicePixelRatio(dpi);                            \
    QPainter p(&temp);                                        \
    p.setRenderHint(QPainter::Antialiasing, true);            \
    p.setPen(Pen);                                            \
    p.setBrush(Brush);                                        \
    p.drawLine(QPointF{4.5, 9}, QPointF{4.5, 1.5});           \
    p.drawLine(QPointF{2.5, 3}, QPointF{3.5, 2});             \
    p.drawLine(QPointF{6.5, 4}, QPointF{5.5, 3});             \
    Image = QPixmap::fromImage(temp);                         \
  } while(0)

    auto& skin = Process::Style::instance();
    const auto& nobrush = skin.NoBrush();
    for(int i = 0; i < 5; i++)
    {
      auto& pens = pen_sets[i];

      // clang-format off
      DRAW_ELLIPSE(SmallEllipsesIn[i], pens.pen_base, nobrush, smallEllipse);
      DRAW_ELLIPSE(LargeEllipsesIn[i], pens.pen_base, nobrush, largeEllipse);
      DRAW_ELLIPSE(SmallEllipsesOut[i], pens.pen_base, pens.brush_base, smallEllipse);
      DRAW_ELLIPSE(LargeEllipsesOut[i], pens.pen_base, pens.brush_base, largeEllipse);
      DRAW_ELLIPSE(SmallEllipsesInLight[i], pens.pen_light, nobrush, smallEllipse);
      DRAW_ELLIPSE(LargeEllipsesInLight[i], pens.pen_light, nobrush, largeEllipse);
      DRAW_ELLIPSE(SmallEllipsesOutLight[i], pens.pen_light, pens.brush_light, smallEllipse);
      DRAW_ELLIPSE(LargeEllipsesOutLight[i], pens.pen_light, pens.brush_light, largeEllipse);

      DRAW_ARROW_ADDR_INLET(Address_Inlet_NoCable[i], pens.pen_cosmetic_nodot, nobrush);
      DRAW_ARROW_ADDR_INLET(Address_Inlet_Cable[i], pens.pen_cosmetic_dot, nobrush);
      DRAW_ARROW_ADDR_OUTLET(Address_Outlet_NoCable[i], pens.pen_cosmetic_nodot, nobrush);
      DRAW_ARROW_ADDR_OUTLET(Address_Outlet_Cable[i], pens.pen_cosmetic_dot, nobrush);
      // clang-format on
    }

#undef DRAW_ELLIPSE
  }
};
}
Q_GLOBAL_STATIC(const PortStyle, port_skin_p)
const QPixmap& PortItem::portImage(
    Process::PortType t, bool inlet, bool smol, bool light, bool addr) noexcept
{
  auto& pixmap_set = *port_skin_p;
  const int n = pixmap_set.penSet(t);

  if(Q_UNLIKELY(light))
  {
    if(inlet)
    {
      if(smol)
      {
        return pixmap_set.SmallEllipsesInLight[n];
      }
      else
      {
        return pixmap_set.LargeEllipsesInLight[n];
      }
    }
    else
    {
      if(smol)
      {
        return pixmap_set.SmallEllipsesOutLight[n];
      }
      else
      {
        return pixmap_set.LargeEllipsesOutLight[n];
      }
    }
  }
  else
  {
    if(inlet)
    {
      if(smol)
      {
        return pixmap_set.SmallEllipsesIn[n];
      }
      else
      {
        return pixmap_set.LargeEllipsesIn[n];
      }
    }
    else
    {
      if(smol)
      {
        return pixmap_set.SmallEllipsesOut[n];
      }
      else
      {
        return pixmap_set.LargeEllipsesOut[n];
      }
    }
  }
}

struct AddressPropagationItem : public QGraphicsItem
{
public:
  using QGraphicsItem::QGraphicsItem;
  Process::PortType type{};
  bool inlet{};
  bool has_address{};
  bool has_cables{};
  bool has_propagate{};

  QRectF boundingRect() const override { return {0, 0, 10, 10}; }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    if(!has_address && !has_propagate)
      return;

    auto& arrows = *port_skin_p;
    const int n = arrows.penSet(type);
    static constexpr QPointF pos{0, 0};

    if(inlet)
    {
      if(has_address)
      {
        if(has_cables)
        {
          painter->drawPixmap(pos, arrows.Address_Inlet_Cable[n]);
        }
        else
        {
          painter->drawPixmap(pos, arrows.Address_Inlet_NoCable[n]);
        }
      }
    }
    else
    {
      if(has_propagate)
      {
        painter->drawPixmap(pos, arrows.Address_Outlet_NoCable[n]);
      }
      else if(has_address)
      {
        if(has_cables)
        {
          painter->drawPixmap(pos, arrows.Address_Outlet_Cable[n]);
        }
        else
        {
          painter->drawPixmap(pos, arrows.Address_Outlet_NoCable[n]);
        }
      }
    }
  }
};

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
  this->setFlag(QGraphicsItem::ItemClipsToShape, false);
  this->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
  this->setFlag(QGraphicsItem::ItemIsFocusable, true);
  if(!p.description().isEmpty())
    this->setToolTip(p.description());
  else if(!p.exposed().isEmpty())
    this->setToolTip(p.exposed());

  // Port address indicator item
  m_address = new AddressPropagationItem{this};
  m_address->type = p.type();
  m_address->inlet = bool(m_inlet);
  m_address->has_address = p.address().isSet();
  m_address->has_cables = !p.cables().empty();
  connect(
      &m_port, &Process::Port::addressChanged, this,
      [a = m_address](const State::AddressAccessor& st) {
    a->has_address = st.isSet();
    a->update();
  });
  connect(&m_port, &Process::Port::cablesChanged, this, [port = &m_port, a = m_address] {
    a->has_cables = port->cables().size() > 0;
    a->update();
  });
  if(!m_inlet && m_port.type() == Process::PortType::Audio)
  {
    auto ap = static_cast<const Process::AudioOutlet*>(&m_port);
    m_address->has_propagate = ap->propagate();
    connect(
        ap, &Process::AudioOutlet::propagateChanged, this,
        [a = m_address](bool propagate) {
      a->has_propagate = propagate;
      a->update();
    });
  }

  if(m_inlet)
    m_address->setPos(QPointF{-4., 2.});
  else
    m_address->setPos(QPointF{7., -1.});

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
      if(!menu->actions().empty())
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
    if(!cable)
      continue;
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
  const QPixmap& img = portImage(
      m_port.type(), m_inlet, m_diam == 8., m_highlight, m_port.address().isSet());
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
        if((portDragDirection == DragSourceIsInlet && !port->m_inlet)
           || (portDragDirection == DragSourceIsOutlet && port->m_inlet))
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
    if((portDragDirection == DragSourceIsInlet && !this->m_inlet)
       || (portDragDirection == DragSourceIsOutlet && this->m_inlet))
    {
      prepareGeometryChange();
      m_diam = 12.;
      update();
    }
  }
  else
  {
    auto fmt = event->mimeData()->formats();
    if(fmt.contains(score::mime::messagelist()) || fmt.contains(score::mime::nodelist()))
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
  auto text = port.visualName();
  if(text.isEmpty())
  {
    if(qobject_cast<const Process::Inlet*>(&port))
    {
      switch(port.type())
      {
        case Process::PortType::Audio:
          text = QStringLiteral("Audio In");
          break;
        default:
        case Process::PortType::Message:
          text = QStringLiteral("Value In");
          break;
        case Process::PortType::Midi:
          text = QStringLiteral("MIDI In");
          break;
        case Process::PortType::Texture:
          text = QStringLiteral("Texture In");
          break;
        case Process::PortType::Geometry:
          text = QStringLiteral("Geometry In");
          break;
      }
    }
    else
    {
      switch(port.type())
      {
        case Process::PortType::Audio:
          text = QStringLiteral("Audio Out");
          break;
        default:
        case Process::PortType::Message:
          text = QStringLiteral("Value Out");
          break;
        case Process::PortType::Midi:
          text = QStringLiteral("MIDI Out");
          break;
        case Process::PortType::Texture:
          text = QStringLiteral("Texture Out");
          break;
        case Process::PortType::Geometry:
          text = QStringLiteral("Geometry Out");
          break;
      }
    }
  }

  lab->setText(text);

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
