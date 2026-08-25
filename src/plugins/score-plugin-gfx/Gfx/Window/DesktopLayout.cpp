#include "DesktopLayout.hpp"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QScreen>

namespace Gfx
{

// Colors matching OutputPreview's previewColorForIndex
static QColor desktopItemColor(int index)
{
  static constexpr QColor colors[] = {
      QColor(30, 60, 180),  QColor(180, 30, 30),  QColor(30, 150, 30),
      QColor(180, 150, 30), QColor(150, 30, 150), QColor(30, 150, 150),
      QColor(200, 100, 30), QColor(100, 30, 200),
  };
  return colors[index % 8];
}

// --- DesktopLayoutItem ---

DesktopLayoutItem::DesktopLayoutItem(
    int index, const QRectF& rect, DesktopLayoutCanvas* canvas,
    QGraphicsItem* parent)
    : QGraphicsRectItem(rect, parent)
    , m_index{index}
    , m_canvas{canvas}
{
  setFlags(
      QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable
      | QGraphicsItem::ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);
  setZValue(10); // Above screen rects
}

void DesktopLayoutItem::setOutputIndex(int idx)
{
  m_index = idx;
  update();
}

void DesktopLayoutItem::applyLockedState()
{
  if(lockMode == OutputLockMode::FullLock)
  {
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(false);
  }
  else
  {
    setFlags(
        QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable
        | QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
  }
  update();
}

void DesktopLayoutItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  // Style matching OutputMappingItem
  const bool isLocked = (lockMode == OutputLockMode::FullLock);
  if(isSelected())
  {
    setZValue(20);
    if(isLocked)
      setBrush(QBrush(QColor(180, 200, 255, 100)));
    else
      setBrush(QBrush(QColor(140, 190, 255, 120)));
    setPen(QPen(Qt::yellow, 2));
  }
  else
  {
    setZValue(10);
    if(isLocked)
      setBrush(QBrush(QColor(80, 80, 80, 60)));
    else
      setBrush(QBrush(QColor(100, 150, 255, 80)));
    setPen(QPen(Qt::white, 1));
  }

  QGraphicsRectItem::paint(painter, option, widget);

  // Index label (small, like input mapping)
  painter->setPen(isLocked ? QColor(180, 180, 180) : QColor(Qt::white));
  auto font = painter->font();
  font.setPixelSize(14);
  painter->setFont(font);
  QString label = QString::number(m_index);
  static constexpr const char* lockLabels[]
      = {nullptr, " [AR]", " [1:1]", " [L]"};
  if(int(lockMode) > 0 && int(lockMode) <= 3)
    label += QString::fromLatin1(lockLabels[int(lockMode)]);
  painter->drawText(rect(), Qt::AlignCenter, label);
}

int DesktopLayoutItem::hitTestEdges(const QPointF& pos) const
{
  constexpr double margin = 6.0;
  int edges = None;
  auto r = rect();
  if(pos.x() - r.left() < margin)
    edges |= Left;
  if(r.right() - pos.x() < margin)
    edges |= Right;
  if(pos.y() - r.top() < margin)
    edges |= Top;
  if(r.bottom() - pos.y() < margin)
    edges |= Bottom;
  return edges;
}

QVariant DesktopLayoutItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemPositionChange && m_canvas && m_canvas->snapEnabled())
  {
    return m_canvas->snapPosition(this, value.toPointF());
  }
  if(change == ItemPositionHasChanged)
  {
    if(onChanged)
      onChanged();
  }
  return QGraphicsRectItem::itemChange(change, value);
}

void DesktopLayoutItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(lockMode == OutputLockMode::FullLock)
  {
    QGraphicsRectItem::mousePressEvent(event);
    return;
  }

  // OneToOne and AspectRatio modes prevent free resize via edges
  const bool sizeFixed = (lockMode == OutputLockMode::OneToOne);
  m_resizeEdges = sizeFixed ? None : hitTestEdges(event->pos());
  if(m_resizeEdges != None)
  {
    m_dragStart = event->scenePos();
    m_rectStart = rect();
    event->accept();
  }
  else
  {
    m_moveAnchorScene = event->scenePos();
    m_posAtPress = pos();
    QGraphicsRectItem::mousePressEvent(event);
  }
}

void DesktopLayoutItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(lockMode == OutputLockMode::FullLock)
  {
    event->accept();
    return;
  }

  if(m_resizeEdges != None)
  {
    auto delta = event->scenePos() - m_dragStart;
    QRectF r = m_rectStart;
    constexpr double minSize = 5.0;

    if(m_resizeEdges & Left)
    {
      double newLeft = qMin(r.left() + delta.x(), r.right() - minSize);
      r.setLeft(newLeft);
    }
    if(m_resizeEdges & Right)
    {
      double newRight = qMax(r.right() + delta.x(), r.left() + minSize);
      r.setRight(newRight);
    }
    if(m_resizeEdges & Top)
    {
      double newTop = qMin(r.top() + delta.y(), r.bottom() - minSize);
      r.setTop(newTop);
    }
    if(m_resizeEdges & Bottom)
    {
      double newBottom = qMax(r.bottom() + delta.y(), r.top() + minSize);
      r.setBottom(newBottom);
    }

    // Enforce aspect ratio: adjust height to match width
    if(lockMode == OutputLockMode::AspectRatio && aspectRatio > 0.0)
    {
      double h = r.width() / aspectRatio;
      r.setHeight(qMax(h, minSize));
    }

    setRect(r);
    if(onChanged)
      onChanged();
    event->accept();
  }
  else
  {
    auto rawDelta = event->scenePos() - m_moveAnchorScene;
    setPos(m_posAtPress + rawDelta);
  }
}

void DesktopLayoutItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_resizeEdges = None;
  QGraphicsRectItem::mouseReleaseEvent(event);
}

void DesktopLayoutItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  if(lockMode == OutputLockMode::FullLock)
  {
    setCursor(Qt::ArrowCursor);
    QGraphicsRectItem::hoverMoveEvent(event);
    return;
  }

  if(lockMode == OutputLockMode::OneToOne)
  {
    // No resize cursors, only move
    setCursor(Qt::SizeAllCursor);
    QGraphicsRectItem::hoverMoveEvent(event);
    return;
  }

  int edges = hitTestEdges(event->pos());
  if((edges & Left) && (edges & Top))
    setCursor(Qt::SizeFDiagCursor);
  else if((edges & Right) && (edges & Bottom))
    setCursor(Qt::SizeFDiagCursor);
  else if((edges & Left) && (edges & Bottom))
    setCursor(Qt::SizeBDiagCursor);
  else if((edges & Right) && (edges & Top))
    setCursor(Qt::SizeBDiagCursor);
  else if(edges & (Left | Right))
    setCursor(Qt::SizeHorCursor);
  else if(edges & (Top | Bottom))
    setCursor(Qt::SizeVerCursor);
  else
    setCursor(Qt::SizeAllCursor);

  QGraphicsRectItem::hoverMoveEvent(event);
}

// --- DesktopLayoutCanvas ---

DesktopLayoutCanvas::DesktopLayoutCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
  setScene(&m_scene);
  setMinimumSize(300, 200);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setBackgroundBrush(QBrush(QColor(30, 30, 30)));
  setRenderHint(QPainter::Antialiasing);

  connect(&m_scene, &QGraphicsScene::selectionChanged, this, [this] {
    if(onSelectionChanged)
    {
      auto items = m_scene.selectedItems();
      if(!items.isEmpty())
      {
        if(auto* item = dynamic_cast<DesktopLayoutItem*>(items.first()))
          onSelectionChanged(item->outputIndex());
      }
      else
      {
        onSelectionChanged(-1);
      }
    }
  });

  // Listen for screen changes
  connect(qApp, &QGuiApplication::screenAdded, this, [this] { refreshScreens(); });
  connect(qApp, &QGuiApplication::screenRemoved, this, [this] { refreshScreens(); });

  refreshScreens();
}

DesktopLayoutCanvas::~DesktopLayoutCanvas()
{
  onSelectionChanged = nullptr;
  onItemGeometryChanged = nullptr;
}

void DesktopLayoutCanvas::refreshScreens()
{
  // Compute virtual desktop bounding rect from all screens
  const auto screens = qApp->screens();
  if(screens.isEmpty())
    return;

  QRect bounds;
  for(auto* screen : screens)
    bounds = bounds.united(screen->geometry());

  m_virtualDesktopBounds = QRectF(bounds);

  // Scale to fit ~500px on the longest axis, with padding
  constexpr double maxDim = 500.0;
  constexpr double padding = 20.0;
  double longest = qMax(bounds.width(), bounds.height());
  m_scaleFactor = (maxDim - 2.0 * padding) / longest;

  double sceneW = bounds.width() * m_scaleFactor + 2.0 * padding;
  double sceneH = bounds.height() * m_scaleFactor + 2.0 * padding;

  m_sceneOffset = QPointF(padding, padding);

  m_scene.setSceneRect(0, 0, sceneW, sceneH);

  rebuildScreenRects();
  fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void DesktopLayoutCanvas::rebuildScreenRects()
{
  // Remove old screen visuals
  for(auto* r : m_screenRects)
  {
    m_scene.removeItem(r);
    delete r;
  }
  m_screenRects.clear();
  for(auto* t : m_screenLabels)
  {
    m_scene.removeItem(t);
    delete t;
  }
  m_screenLabels.clear();

  const auto screens = qApp->screens();
  for(int i = 0; i < screens.size(); ++i)
  {
    auto* screen = screens[i];
    QRectF geo = screen->geometry();

    // Map to scene coords
    QRectF sceneRect(
        desktopToScene(geo.topLeft()),
        desktopSizeToScene(geo.size().toSize()));

    auto* rect = m_scene.addRect(sceneRect, QPen(QColor(80, 80, 80), 1));
    rect->setBrush(QBrush(QColor(50, 50, 50)));
    rect->setZValue(0);
    m_screenRects.push_back(rect);

    // Label: screen name + resolution
    QString label = QStringLiteral("%1\n%2x%3")
                        .arg(screen->name())
                        .arg((int)geo.width())
                        .arg((int)geo.height());
    auto* text = m_scene.addSimpleText(label);
    text->setBrush(QColor(120, 120, 120));
    auto font = text->font();
    font.setPixelSize(10);
    text->setFont(font);
    text->setPos(sceneRect.left() + 4, sceneRect.top() + 4);
    text->setZValue(1);
    m_screenLabels.push_back(text);
  }
}

QPointF DesktopLayoutCanvas::desktopToScene(QPointF desktopPos) const
{
  return (desktopPos - m_virtualDesktopBounds.topLeft()) * m_scaleFactor + m_sceneOffset;
}

QPointF DesktopLayoutCanvas::sceneToDesktop(QPointF scenePos) const
{
  return (scenePos - m_sceneOffset) / m_scaleFactor + m_virtualDesktopBounds.topLeft();
}

QSizeF DesktopLayoutCanvas::desktopSizeToScene(QSize desktopSize) const
{
  return QSizeF(desktopSize.width() * m_scaleFactor, desktopSize.height() * m_scaleFactor);
}

QSize DesktopLayoutCanvas::sceneSizeToDesktop(QSizeF sceneSize) const
{
  return QSize(
      qMax(1, (int)(sceneSize.width() / m_scaleFactor)),
      qMax(1, (int)(sceneSize.height() / m_scaleFactor)));
}

int DesktopLayoutCanvas::detectScreen(QRect windowRect) const
{
  const auto screens = qApp->screens();
  int bestIndex = -1;
  int bestArea = 0;

  for(int i = 0; i < screens.size(); ++i)
  {
    QRect overlap = screens[i]->geometry().intersected(windowRect);
    if(!overlap.isEmpty())
    {
      int area = overlap.width() * overlap.height();
      if(area > bestArea)
      {
        bestArea = area;
        bestIndex = i;
      }
    }
  }
  return bestIndex;
}

void DesktopLayoutCanvas::setupItemCallbacks(DesktopLayoutItem* item)
{
  item->onChanged = [this, item] {
    if(onItemGeometryChanged)
      onItemGeometryChanged(item->outputIndex());
  };
}

void DesktopLayoutCanvas::setWindowItems(const std::vector<OutputMapping>& mappings)
{
  // Remove existing window items
  QList<QGraphicsItem*> toRemove;
  for(auto* item : m_scene.items())
    if(dynamic_cast<DesktopLayoutItem*>(item))
      toRemove.append(item);
  for(auto* item : toRemove)
  {
    m_scene.removeItem(item);
    delete item;
  }

  for(int i = 0; i < (int)mappings.size(); ++i)
  {
    const auto& m = mappings[i];
    QPointF scenePos = desktopToScene(QPointF(m.windowPosition));
    QSizeF sceneSize = desktopSizeToScene(m.windowSize);
    QRectF sceneRect(0, 0, sceneSize.width(), sceneSize.height());

    auto* item = new DesktopLayoutItem(i, sceneRect, this);
    item->setPos(scenePos);
    item->lockMode = m.lockMode;
    // Compute aspect ratio from source rect
    double srcW = m.sourceRect.width();
    double srcH = m.sourceRect.height();
    if(srcH > 0.0 && srcW > 0.0)
      item->aspectRatio = srcW / srcH;
    item->applyLockedState();
    setupItemCallbacks(item);
    m_scene.addItem(item);
  }
}

void DesktopLayoutCanvas::addOutput(QPoint pos, QSize size)
{
  int count = 0;
  for(auto* item : m_scene.items())
    if(dynamic_cast<DesktopLayoutItem*>(item))
      count++;

  QPointF scenePos = desktopToScene(QPointF(pos));
  QSizeF sceneSize = desktopSizeToScene(size);
  QRectF sceneRect(0, 0, sceneSize.width(), sceneSize.height());

  auto* item = new DesktopLayoutItem(count, sceneRect, this);
  item->setPos(scenePos);
  setupItemCallbacks(item);
  m_scene.addItem(item);

  m_scene.clearSelection();
  item->setSelected(true);
}

void DesktopLayoutCanvas::removeOutput(int index)
{
  QList<DesktopLayoutItem*> items;
  for(auto* item : m_scene.items())
    if(auto* di = dynamic_cast<DesktopLayoutItem*>(item))
      items.append(di);

  // Remove the item with this index
  for(auto* di : items)
  {
    if(di->outputIndex() == index)
    {
      m_scene.removeItem(di);
      delete di;
      break;
    }
  }

  // Re-index remaining items
  items.clear();
  for(auto* item : m_scene.items())
    if(auto* di = dynamic_cast<DesktopLayoutItem*>(item))
      items.append(di);

  std::sort(items.begin(), items.end(), [](auto* a, auto* b) {
    return a->outputIndex() < b->outputIndex();
  });

  for(int i = 0; i < items.size(); ++i)
    items[i]->setOutputIndex(i);
}

void DesktopLayoutCanvas::updateItem(int index, QPoint pos, QSize size)
{
  for(auto* item : m_scene.items())
  {
    if(auto* di = dynamic_cast<DesktopLayoutItem*>(item))
    {
      if(di->outputIndex() == index)
      {
        auto savedCallback = std::move(di->onChanged);
        di->onChanged = nullptr;
        di->setPos(desktopToScene(QPointF(pos)));
        QSizeF sceneSize = desktopSizeToScene(size);
        di->setRect(QRectF(0, 0, sceneSize.width(), sceneSize.height()));
        di->onChanged = std::move(savedCallback);
        return;
      }
    }
  }
}

void DesktopLayoutCanvas::selectItem(int index)
{
  m_scene.clearSelection();
  for(auto* item : m_scene.items())
  {
    if(auto* di = dynamic_cast<DesktopLayoutItem*>(item))
    {
      if(di->outputIndex() == index)
      {
        di->setSelected(true);
        return;
      }
    }
  }
}

void DesktopLayoutCanvas::resizeEvent(QResizeEvent* event)
{
  QGraphicsView::resizeEvent(event);
  fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void DesktopLayoutCanvas::setSnapEnabled(bool enabled)
{
  m_snapEnabled = enabled;
}

QPointF
DesktopLayoutCanvas::snapPosition(const DesktopLayoutItem* item, QPointF pos) const
{
  // Snap threshold in scene coords (fixed visual distance)
  const double threshold = 8.0;

  QRectF itemRect(pos, item->rect().size());

  // Collect snap edges: screen borders + other window borders
  std::vector<double> hEdges, vEdges;

  for(auto* sr : m_screenRects)
  {
    auto r = sr->rect();
    hEdges.push_back(r.left());
    hEdges.push_back(r.right());
    vEdges.push_back(r.top());
    vEdges.push_back(r.bottom());
  }

  for(auto* gi : m_scene.items())
  {
    auto* other = dynamic_cast<const DesktopLayoutItem*>(gi);
    if(!other || other == item)
      continue;
    auto r = other->mapRectToScene(other->rect());
    hEdges.push_back(r.left());
    hEdges.push_back(r.right());
    vEdges.push_back(r.top());
    vEdges.push_back(r.bottom());
  }

  double bestDx = threshold + 1;
  double snapX = pos.x();

  // Snap left edge or right edge of item to any vertical edge
  for(double edge : hEdges)
  {
    double dLeft = std::abs(itemRect.left() - edge);
    double dRight = std::abs(itemRect.right() - edge);
    if(dLeft < bestDx)
    {
      bestDx = dLeft;
      snapX = edge;
    }
    if(dRight < bestDx)
    {
      bestDx = dRight;
      snapX = edge - itemRect.width();
    }
  }

  double bestDy = threshold + 1;
  double snapY = pos.y();

  for(double edge : vEdges)
  {
    double dTop = std::abs(itemRect.top() - edge);
    double dBottom = std::abs(itemRect.bottom() - edge);
    if(dTop < bestDy)
    {
      bestDy = dTop;
      snapY = edge;
    }
    if(dBottom < bestDy)
    {
      bestDy = dBottom;
      snapY = edge - itemRect.height();
    }
  }

  return QPointF(
      bestDx <= threshold ? snapX : pos.x(), bestDy <= threshold ? snapY : pos.y());
}

}
