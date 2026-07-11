// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequenceView)

static constexpr double k_handleHalfWidth = 5.0;
static constexpr double k_handleZValue = 10.0;

// ---------------------------------------------------------------------------
// SequenceHandleItem — one vertical drag handle per intermediate IS position.
// Lives as a child of SequenceView at the appropriate x offset.
// Has its own mouse handling so it captures events even when TemporalIntervalView
// siblings (which default to Qt::AllButtons) overlap the same pixel column.
// ---------------------------------------------------------------------------
class SequenceHandleItem final : public QGraphicsItem
{
public:
  Id<Scenario::TimeSyncModel> tsId;

  explicit SequenceHandleItem(Sequence::SequenceView* parent)
      : QGraphicsItem{parent}
  {
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(k_handleZValue);
  }

  void setHeight(qreal h)
  {
    if(h == m_height)
      return;
    prepareGeometryChange();
    m_height = h;
  }

  QRectF boundingRect() const override
  {
    // Extra top margin for the state dot (radius 4.5 * 2 = 9px)
    return {-k_handleHalfWidth, -9., k_handleHalfWidth * 2., m_height + 9.};
  }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    static constexpr qreal dotR = 4.5;
    auto& style = Process::Style::instance();
    const auto& pen = m_active ? style.skin.Base3.main.pen2 : style.skin.Base2.main.pen1;
    p->setPen(pen);
    p->drawLine(QPointF{0., 0.}, QPointF{0., m_height});
    // State indicator dot at top
    p->setPen(style.NoPen());
    p->setBrush(pen.color());
    p->drawEllipse(QPointF{0., dotR}, dotR, dotR);
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override
  {
    m_active = true;
    // Ripple mode is decided at press time and kept for the whole drag, so a
    // single gesture never mixes two different ongoing commands.
    m_ripple = e->modifiers().testFlag(Qt::ShiftModifier);
    update();
    e->accept();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override
  {
    auto* sv = static_cast<Sequence::SequenceView*>(parentItem());
    const double newX = qBound(0., mapToParent(e->pos()).x(), sv->width());
    setX(newX);
    sv->handleDragMoved(tsId, newX, m_ripple);
    e->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override
  {
    auto* sv = static_cast<Sequence::SequenceView*>(parentItem());
    const double finalX = qBound(0., mapToParent(e->pos()).x(), sv->width());
    setX(finalX);
    m_active = false;
    update();
    sv->handleDragReleased(tsId, finalX, m_ripple);
    e->accept();
  }

  void keyPressEvent(QKeyEvent* e) override
  {
    if(e->key() == Qt::Key_Escape && m_active)
    {
      m_active = false;
      update();
      static_cast<Sequence::SequenceView*>(parentItem())->handleDragCancelled();
      e->accept();
    }
    else
    {
      e->ignore();
    }
  }

private:
  qreal m_height{100.};
  bool m_active{false};
  bool m_ripple{false};
};

// ---------------------------------------------------------------------------
// SequenceRailItem — dedicated strip at the top of the sequence layer where
// the IS handles' dots live. Double-clicking it inserts a new IS.
// ---------------------------------------------------------------------------
class SequenceRailItem final : public QGraphicsItem
{
public:
  explicit SequenceRailItem(Sequence::SequenceView* parent)
      : QGraphicsItem{parent}
  {
    setAcceptedMouseButtons(Qt::LeftButton);
    // Below the handles (z=10) so they keep receiving their drags.
    setZValue(k_handleZValue - 1.);
  }

  void setWidth(qreal w)
  {
    if(w == m_width)
      return;
    prepareGeometryChange();
    m_width = w;
  }

  QRectF boundingRect() const override
  {
    return {0., 0., m_width, Sequence::SequenceView::RailHeight};
  }

  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    auto& style = Process::Style::instance();
    p->setPen(style.NoPen());
    p->setBrush(style.skin.Background1.main.brush);
    p->drawRect(boundingRect());
    p->setPen(style.skin.Base2.darker.pen1);
    const double midY = Sequence::SequenceView::RailHeight / 2.;
    p->drawLine(QPointF{0., midY}, QPointF{m_width, midY});
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override { e->accept(); }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override { e->accept(); }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override
  {
    static_cast<Sequence::SequenceView*>(parentItem())
        ->railDoubleClicked(e->pos().x());
    e->accept();
  }

private:
  qreal m_width{100.};
};

// ---------------------------------------------------------------------------

namespace Sequence
{

SequenceView::SequenceView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
  setAcceptedMouseButtons(Qt::LeftButton);
  m_rail = new SequenceRailItem{this};
}

SequenceView::~SequenceView() = default;

void SequenceView::setHandles(const QVector<HandleData>& handles)
{
  m_handles = handles;

  while(m_handleLines.size() > handles.size())
    delete m_handleLines.takeLast();

  while(m_handleLines.size() < handles.size())
    m_handleLines.append(new SequenceHandleItem{this});

  updateHandleLines();
  update();
}

void SequenceView::updateHandleLines()
{
  m_rail->setWidth(width());

  const qreal h = height();
  for(int i = 0; i < m_handles.size(); ++i)
  {
    m_handleLines[i]->tsId = m_handles[i].tsId;
    m_handleLines[i]->setX(m_handles[i].x);
    m_handleLines[i]->setY(0.);
    m_handleLines[i]->setHeight(h);
  }
}

void SequenceView::paint_impl(QPainter* p) const
{
  auto& style = Process::Style::instance();
  p->setPen(style.NoPen());
  p->setBrush(style.skin.Background2.main.brush);
  p->drawRect(boundingRect());
}

} // namespace Sequence
