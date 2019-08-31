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
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <QGraphicsScene>
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

std::array<QImage, 3> SmallEllipsesIn;
std::array<QImage, 3> LargeEllipsesIn;

std::array<QImage, 3> SmallEllipsesOut;
std::array<QImage, 3> LargeEllipsesOut;

static const QImage& portImage(Process::PortType t, bool inlet, bool small) noexcept
{
  int n;
  switch(t) {
    case Process::PortType::Audio: n = 0; break;
    case Process::PortType::Midi: n = 2; break;
    default: n = 1; break;
  };

  if(inlet)
  {
    if(small)
    {
      return SmallEllipsesIn[n];
    }
    else
    {
      return LargeEllipsesIn[n];
    }
  }
  else
  {
    if(small)
    {
      return SmallEllipsesOut[n];
    }
    else
    {
      return LargeEllipsesOut[n];
    }
  }
}

// TODO move in ProcessStyle
static bool initEllipses(Process::Style& skin)
{
  static constexpr qreal smallRadius = 3.;
  static constexpr qreal largeRadius = 5.;
  static constexpr QRectF smallEllipse{3., 3., 2. * smallRadius, 2. * smallRadius};
  static constexpr QRectF largeEllipse{1., 1., 2. * largeRadius, 2. * largeRadius};
  const qreal dpi = qApp->devicePixelRatio();
  const qreal sz = dpi * 13.;

#define DRAW_ELLIPSE(Image, Pen, Brush, Ellipse) \
  do { \
    Image = QImage(sz, sz, QImage::Format_ARGB32_Premultiplied); \
    Image.fill(Qt::transparent); \
    Image.setDevicePixelRatio(dpi); \
    QPainter p(&Image); \
    p.setRenderHint(QPainter::Antialiasing, true); \
    p.setPen(Pen); \
    p.setBrush(Brush); \
    p.drawEllipse(Ellipse); \
  } while(0)

  const auto& audiopen = skin.AudioPortPen();
  const auto& datapen = skin.DataPortPen();
  const auto& midipen = skin.MidiPortPen();
  const auto& audiobrush = skin.skin.Port1.main.brush;
  const auto& databrush = skin.skin.Port2.main.brush;
  const auto& midibrush = skin.skin.Port3.main.brush;
  const auto& nobrush = skin.NoBrush();
  DRAW_ELLIPSE(SmallEllipsesIn[0], audiopen, nobrush, smallEllipse);
  DRAW_ELLIPSE(SmallEllipsesIn[1], datapen, nobrush, smallEllipse);
  DRAW_ELLIPSE(SmallEllipsesIn[2], midipen, nobrush, smallEllipse);
  DRAW_ELLIPSE(SmallEllipsesOut[0], audiopen, audiobrush, smallEllipse);
  DRAW_ELLIPSE(SmallEllipsesOut[1], datapen, databrush, smallEllipse);
  DRAW_ELLIPSE(SmallEllipsesOut[2], midipen, midibrush, smallEllipse);

  DRAW_ELLIPSE(LargeEllipsesIn[0], audiopen, nobrush, largeEllipse);
  DRAW_ELLIPSE(LargeEllipsesIn[1], datapen, nobrush, largeEllipse);
  DRAW_ELLIPSE(LargeEllipsesIn[2], midipen, nobrush, largeEllipse);
  DRAW_ELLIPSE(LargeEllipsesOut[0], audiopen, audiobrush, largeEllipse);
  DRAW_ELLIPSE(LargeEllipsesOut[1], datapen, databrush, largeEllipse);
  DRAW_ELLIPSE(LargeEllipsesOut[2], midipen, midibrush, largeEllipse);

#undef DRAW_ELLIPSE
  return true;
}


PortItem* PortItem::clickedPort;
PortItem::PortItem(
    Process::Port& p,
    const Process::ProcessPresenterContext& ctx,
    QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_context{ctx}
  , m_port{p}
  , m_diam{8.}
  , m_inlet{bool(qobject_cast<Process::Inlet*>(&p))}
{
  [[maybe_unused]]
  static bool init = initEllipses(Process::Style::instance());
  this->setCursor(QCursor(Qt::PointingHandCursor));
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
  this->setToolTip(p.customData());

  con(p.selection, &Selectable::changed,
      this, [this] (bool b) {
    for(const auto& cable : cables) {
      cable->setZValue(b || cable->model().selection.get() ? 999999 : -1);
    }
  });
  auto plug = ctx.findPlugin<Process::DocumentPlugin>();
  if(plug)
  {
    plug->ports().insert({&p, this});

    Path<Process::Port> path = p;
    for (auto c : plug->cables())
    {
      if (c.first->source().unsafePath() == path.unsafePath())
      {
        c.second->setSource(this);
        cables.push_back(c.second);
      }
      else if (c.first->sink().unsafePath() == path.unsafePath())
      {
        c.second->setTarget(this);
        cables.push_back(c.second);
      }
    }

    QObject::connect(
        this,
        &Dataflow::PortItem::contextMenuRequested,
        this,
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
  for (auto cable : cables)
  {
    if (cable->source() == this)
      cable->setSource(nullptr);
    if (cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto& ctx = m_context;
  auto plug = ctx.findPlugin<Process::DocumentPlugin>();
  if(!plug)
    return;
  auto& p = plug->ports();
  auto it = p.find(&m_port);
  if (it != p.end())
    p.erase(it);
}

void PortItem::setupMenu(QMenu&, const score::DocumentContext& ctx) {}

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

QRectF PortItem::boundingRect() const
{
  constexpr auto max_diam = 13.;
  return {0., 0., max_diam, max_diam};
}
void PortItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  const QImage& img = portImage(m_port.type, m_inlet, m_diam == 8.);
  painter->drawImage(0, 0, img);

  /*

  painter->setRenderHint(QPainter::Antialiasing, true);

  auto& style = Process::Style::instance();
  switch (m_port.type)
  {
    case Process::PortType::Audio:
      painter->setPen(style.AudioPortPen());
      painter->setBrush(m_inlet ? style.AudioPortBrush().brush : style.skin.NoBrush);
      break;
    case Process::PortType::Message:
      painter->setPen(style.DataPortPen());
      painter->setBrush(m_inlet ? style.DataPortBrush().brush : style.skin.NoBrush);
      break;
    case Process::PortType::Midi:
      painter->setPen(style.MidiPortPen());
      painter->setBrush(m_inlet ? style.MidiPortBrush().brush : style.skin.NoBrush);
      break;
  }

  painter->drawPolygon(m_diam == 8. ? smallEllipsePath : largeEllipsePath);
  painter->setRenderHint(QPainter::Antialiasing, false);*/
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

QLineF portDragLineCoords{};
struct DragLine : QGraphicsLineItem
{
public:
  DragLine(QLineF f)
    : QGraphicsLineItem{f}
  {
    setPen(QPen(QBrush{qRgb(200, 200, 210)}, 2, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap,
                Qt::PenJoinStyle::RoundJoin));

    setAcceptHoverEvents(true);
  }
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    portDragLineCoords.setP2(event->scenePos());
    setLine(portDragLineCoords);
  }
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
  {
    portDragLineCoords.setP2(event->scenePos());
    setLine(portDragLineCoords);
  }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    portDragLineCoords.setP2(event->scenePos());
    setLine(portDragLineCoords);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    QGraphicsLineItem::paint(painter, option, widget);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
};

QGraphicsLineItem* portDragLine{};
struct DragMoveFilter : QObject
{
public:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if (event->type() == QEvent::GraphicsSceneDragMove) {
        auto ev = static_cast<QGraphicsSceneDragDropEvent *>(event);
        portDragLineCoords.setP2(ev->scenePos());
        portDragLine->setLine(portDragLineCoords);
        return false;
    } else {
        return QObject::eventFilter(watched, event);
    }
  }
};
DragMoveFilter* drag_move_filter{};
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
        score::SelectionDispatcher{
            score::IDocument::documentContext(m_port).selectionStack}
            .setAndCommit({&m_port});
        break;
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

QVariant PortItem::itemChange(
    QGraphicsItem::GraphicsItemChange change,
    const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      for (auto cbl : cables)
      {
        cbl->resize();
      }
      break;
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for (auto cbl : cables)
      {
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
  switch(type)
  {
  case Process::PortType::Audio: return skin.Port1;
  case Process::PortType::Message: return skin.Port2;
  case Process::PortType::Midi: return skin.Port3;
  default: return skin.Warn1;
  }
}

}
