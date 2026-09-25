#include "GraphicsStripDetailLayout.hpp"

#include <score/model/Skin.hpp>

#include <QFontMetricsF>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <QPainter>

namespace score
{
namespace
{
// Layouts let clicks through: catch them under the content
class StripCellHitArea final : public QGraphicsItem
{
public:
  explicit StripCellHitArea(GraphicsStripCell* cell)
      : QGraphicsItem{cell}
      , m_cell{*cell}
  {
    setZValue(-1.);
    setCursor(Qt::PointingHandCursor);
  }

  QRectF boundingRect() const override { return m_cell.rect(); }
  void resized() { prepareGeometryChange(); }
  void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override { }

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override
  {
    const bool ok = event->mimeData() && m_cell.acceptsDrop(*event->mimeData());
    event->setAccepted(ok);
    m_cell.setDropHighlight(ok);
  }
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override
  {
    m_cell.setDropHighlight(false);
    event->accept();
  }
  void dropEvent(QGraphicsSceneDragDropEvent* event) override
  {
    m_cell.setDropHighlight(false);
    if(event->mimeData() && m_cell.acceptsDrop(*event->mimeData()))
    {
      m_cell.drop(*event->mimeData());
      event->acceptProposedAction();
    }
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    if(event->button() == Qt::LeftButton && m_cell.onClicked)
    {
      m_cell.onClicked();
      event->accept();
    }
    else
    {
      event->ignore();
    }
  }

private:
  GraphicsStripCell& m_cell;
};
}

GraphicsStripCell::GraphicsStripCell(QGraphicsItem* parent)
    : GraphicsVBoxLayout{parent}
    , m_hitArea{new StripCellHitArea{this}}
{
  setPadding(4.);
  setSpacing(2.);
}

GraphicsStripCell::~GraphicsStripCell() { }

void GraphicsStripCell::setTitle(const QString& title)
{
  m_title = title;
}

void GraphicsStripCell::setShownTitle(const QString& title)
{
  if(title == m_shownTitle)
    return;
  m_shownTitle = title;
  update();
}

void GraphicsStripCell::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

void GraphicsStripCell::setDropHandler(
    std::function<bool(const QMimeData&)> accepts,
    std::function<void(const QMimeData&)> drop)
{
  m_acceptsDrop = std::move(accepts);
  m_drop = std::move(drop);
  m_hitArea->setAcceptDrops(bool(m_drop));
}

bool GraphicsStripCell::acceptsDrop(const QMimeData& data) const
{
  return m_acceptsDrop && m_drop && m_acceptsDrop(data);
}

void GraphicsStripCell::drop(const QMimeData& data)
{
  if(m_drop)
    m_drop(data);
}

void GraphicsStripCell::setDropHighlight(bool on)
{
  m_dropHighlight = on;
  update();
}

qreal GraphicsStripCell::titleHeight() const
{
  return m_title.isEmpty()
             ? 0.
             : QFontMetricsF{score::Skin::instance().Medium8Pt}.height() + 2.;
}

void GraphicsStripCell::layout()
{
  auto items = this->childItems();
  items.removeOne(m_hitArea);
  updateChildrenRects(items);

  double y = m_padding + titleHeight();
  // Room for the start of a longer shown title
  double max_w = m_title.isEmpty()
                     ? 0.
                     : std::max(
                         56., QFontMetricsF{score::Skin::instance().Medium8Pt}
                                  .horizontalAdvance(m_title));
  bool first = true;
  for(auto item : std::as_const(items))
  {
    const auto r = item->boundingRect();
    if(!first)
      y += spacing();
    item->setPos(m_padding, y);
    y += r.height();
    max_w = std::max(max_w, r.width());
    first = false;
  }
  setRect({0., 0., max_w + 2. * m_padding, y + m_padding});
  static_cast<StripCellHitArea*>(m_hitArea)->resized();
}

void GraphicsStripCell::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = rect().adjusted(1., 1., -1., -1.);
  painter->setBrush(
      m_selected ? skin.Background2.lighter.brush : skin.Background2.darker.brush);
  painter->setPen(
      m_dropHighlight ? skin.Base4.main.pen2
      : m_selected    ? skin.Base4.main.pen1
                      : skin.NoPen);
  painter->drawRoundedRect(r, 3., 3.);
  painter->setRenderHint(QPainter::Antialiasing, false);

  if(!m_title.isEmpty())
  {
    const QFont& f = skin.Medium8Pt;
    painter->setFont(f);
    painter->setPen(m_selected ? skin.HalfLight.main.pen1 : skin.LightGray.main.pen1);
    const QFontMetricsF metrics{f};
    const QString& title = m_shownTitle.isEmpty() ? m_title : m_shownTitle;
    painter->drawText(
        QPointF{m_padding, m_padding + metrics.ascent()},
        metrics.elidedText(title, Qt::ElideRight, rect().width() - 2. * m_padding));
  }
}

GraphicsStripDetailLayout::GraphicsStripDetailLayout(QGraphicsItem* parent)
    : GraphicsLayout{parent}
    , m_strip{new GraphicsHBoxLayout{this}}
{
  m_strip->setSpacing(4.);
}

GraphicsStripDetailLayout::~GraphicsStripDetailLayout() { }

void GraphicsStripDetailLayout::addCell(GraphicsStripCell* cell)
{
  const int index = std::ssize(m_cells);
  cell->setParentItem(m_strip);
  cell->onClicked = [this, index] {
    setCurrentIndex(index);
    if(onCurrentIndexChanged)
      onCurrentIndexChanged(index);
  };
  m_cells.push_back(cell);
}

std::vector<QGraphicsItem*> GraphicsStripDetailLayout::pages() const
{
  std::vector<QGraphicsItem*> ret;
  for(auto item : childItems())
    if(item != m_strip)
      ret.push_back(item);
  return ret;
}

void GraphicsStripDetailLayout::setCurrentIndex(int index)
{
  const auto p = pages();
  if(index < 0 || index >= std::ssize(p))
    return;
  m_currentIndex = index;
  for(int i = 0; i < std::ssize(p); i++)
    p[i]->setVisible(i == index);
  for(int i = 0; i < std::ssize(m_cells); i++)
    m_cells[i]->setSelected(i == index);
}

void GraphicsStripDetailLayout::setStripVisible(bool visible)
{
  m_stripVisible = visible;
  m_strip->setVisible(visible);
}

void GraphicsStripDetailLayout::layout()
{
  const auto p = pages();
  updateChildrenRects(childItems());

  m_strip->setPos(m_padding, m_padding);
  const QRectF sr = m_stripVisible ? m_strip->boundingRect() : QRectF{};
  const double pages_y = m_stripVisible ? m_padding + sr.height() + spacing() : m_padding;

  double w = sr.width();
  double h = 0.;
  for(auto page : p)
  {
    page->setPos(m_padding, pages_y);
    const QRectF r = page->boundingRect();
    w = std::max(w, r.width());
    h = std::max(h, r.height());
  }
  setRect({0., 0., w + 2. * m_padding, pages_y + h + m_padding});
  setCurrentIndex(m_currentIndex);
}
}
