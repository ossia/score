// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <algorithm>
#include <cmath>
#include <iscore/widgets/GraphicsItem.hpp>

#include "TemporalConstraintHeader.hpp"
#include "TemporalConstraintPresenter.hpp"
#include <Scenario/Document/Constraint/ConstraintHeader.hpp>
#include <iscore/model/Skin.hpp>

class QGraphicsSceneMouseEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalConstraintHeader::TemporalConstraintHeader(TemporalConstraintPresenter& pres)
  : ConstraintHeader{}
  , m_presenter{pres}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptDrops(true);
  this->setAcceptedMouseButtons(
      Qt::LeftButton); // needs to be enabled for dblclick
  this->setFlags(
      QGraphicsItem::ItemIsSelectable |
      QGraphicsItem::ItemClipsToShape |
      QGraphicsItem::ItemClipsChildrenToShape);
}

QRectF TemporalConstraintHeader::boundingRect() const
{
  return {0., 0., m_width, qreal(ConstraintHeader::headerHeight())};
}

void TemporalConstraintHeader::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);
  if (m_state == State::RackHidden)
  {
    const auto rect = boundingRect();
    painter->fillRect(
        rect, skin.ConstraintHeaderRackHidden.getBrush());

    // Fake timesync continuation
    painter->setPen(skin.ConstraintHeaderSeparator);
    painter->drawLine(rect.topLeft(), rect.bottomLeft());
    painter->drawLine(rect.topRight(), rect.bottomRight());
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());
    if(m_button)
      m_button->setUnrolled(true);
  }
  else
  {
    painter->setPen(skin.ConstraintHeaderSeparator);
    painter->drawLine(
                QPointF{0., (double)ConstraintHeaderHeight},
                QPointF{m_width, (double)ConstraintHeaderHeight});
    if(m_button)
      m_button->setUnrolled(false);
  }

  // Header
  painter->setPen(skin.ConstraintHeaderTextPen);

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header
  const auto textWidth = m_textRectCache.width();
  auto view = getView(*this);
  int text_left
      = view->mapFromScene(
                mapToScene({m_width / 2. - textWidth / 2., 0.}))
            .x();
  int text_right
      = view->mapFromScene(
                mapToScene({m_width / 2. + textWidth / 2., 0.}))
            .x();
  double x = (m_width - textWidth) / 2.;
  const constexpr double min_x = 10.;
  const double max_x = view->width() - 30.;

  if (text_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - text_left + min_x;
  }
  else if (text_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - text_right + max_x;
  }

  x = std::max(x, 10.);
  if (std::abs(m_previous_x - x) > 0.1)
  {
    m_previous_x = x;
  }

  painter->drawGlyphRun({m_previous_x,
                         (ConstraintHeader::headerHeight() - m_textRectCache.height()) / 2.},
                        *m_line);
}

void TemporalConstraintHeader::updateButtons()
{
  if(m_button)
    m_button->setPos(15, 5);
}

void TemporalConstraintHeader::enableOverlay(bool b)
{
  if(b)
  {
    m_button = new RackButton{this};
    connect(m_button, &RackButton::clicked, &m_presenter,
            [=] { ((TemporalConstraintPresenter&)m_presenter).changeRackState(); });
    updateButtons();
  }
  else
  {
    delete m_button;
    m_button = nullptr;
  }
}

void TemporalConstraintHeader::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent* event)
{
  emit doubleClicked();
}

void TemporalConstraintHeader::on_textChange()
{
  const auto& font = ScenarioStyle::instance().Bold12Pt;
  if(m_text.isEmpty())
  {
    return;
  }
  else
  {
    QTextLayout layout(m_text, font);
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    m_textRectCache = line.naturalTextRect();
    auto r = line.glyphRuns();
    if(r.size() > 0)
      m_line = std::move(r[0]);
    else
      m_line = ossia::none;
  }
}

void TemporalConstraintHeader::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  emit constraintHoverEnter();
}

void TemporalConstraintHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  emit constraintHoverLeave();
}

void TemporalConstraintHeader::dragEnterEvent(
    QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragEnterEvent(event);
  event->accept();
}

void TemporalConstraintHeader::dragLeaveEvent(
    QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  event->accept();
}

void TemporalConstraintHeader::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->pos(), event->mimeData());

  event->accept();
}




RackButton::RackButton(QGraphicsItem* parent):
  QGraphicsObject{parent}
{
}

void RackButton::setUnrolled(bool b)
{
  m_unroll = b;
  update();
}

static const QPainterPath trianglePath{
    [] {
        QPainterPath p;
        QPainterPathStroker s;
        s.setCapStyle(Qt::RoundCap);
        s.setJoinStyle(Qt::RoundJoin);
        s.setWidth(2);

        p.addPolygon(QVector<QPointF>{
                         QPointF(0, 5),
                         QPointF(0, 21),
                         QPointF(9, 13)});
        p.closeSubpath();
        p = QTransform().scale(0.8, 0.8).map(p);

        return p + s.createStroke(p);
    }()
};
static const auto rotatedTriangle = QTransform().rotate(90).translate(8, -12).map(trianglePath);

QRectF RackButton::boundingRect() const
{ return QRectF(trianglePath.boundingRect()); }

void RackButton::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  auto& skin = ScenarioStyle::instance();
  painter->setBrush(skin.ConstraintSolidPen.brush());
  const auto bright = skin.ConstraintSolidPen.brush().color();
  QPen p{bright.darker(150)};
  p.setWidth(2);
  painter->setPen(p);

  if(m_unroll) {
    painter->fillPath(trianglePath, painter->brush());
    painter->drawPath(trianglePath);
  }
  else {
    painter->fillPath(rotatedTriangle, painter->brush());
    painter->drawPath(rotatedTriangle);
  }
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void RackButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  update();
  emit clicked();
}

void RackButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void RackButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  update();
}

}
