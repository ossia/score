#include <Process/Style/ProcessFonts.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <algorithm>
#include <cmath>

#include <Process/Style/Skin.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include "TemporalConstraintHeader.hpp"

class QGraphicsSceneMouseEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalConstraintHeader::TemporalConstraintHeader():
  ConstraintHeader{}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);  // needs to be enabled for dblclick
  this->setFlags(QGraphicsItem::ItemIsSelectable);// needs to be enabled for dblclick

  m_textCache.setCacheEnabled(true);
}

QRectF TemporalConstraintHeader::boundingRect() const
{
  return {0, 0, m_width, qreal(ConstraintHeader::headerHeight())};
}

void TemporalConstraintHeader::paint(
    QPainter *painter,
    const QStyleOptionGraphicsItem *option,
    QWidget *widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
    if(m_state == State::RackHidden)
    {
        auto rect = boundingRect();
        painter->fillRect(rect, ScenarioStyle::instance().ConstraintHeaderRackHidden.getBrush());

        // Fake timenode continuation
        auto color = ScenarioStyle::instance().ConstraintHeaderSideBorder.getColor();
        QPen pen{color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
        painter->setPen(pen);
        painter->drawLine(rect.topLeft(), rect.bottomLeft());
        painter->drawLine(rect.topRight(), rect.bottomRight());
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());
    }

    // Header
    painter->setPen(ScenarioStyle::instance().ConstraintHeaderText.getColor());

    // If the centered text is hidden, we put it at the left so that it's on the view.
    // We have to compute the visible part of the header
    auto view = scene()->views().first();
    int text_left = view->mapFromScene(mapToScene({m_width / 2. - m_textWidthCache / 2., 0})).x();
    int text_right = view->mapFromScene(mapToScene({m_width / 2. + m_textWidthCache / 2., 0})).x();
    double x = (m_width - m_textWidthCache) / 2.;
    double min_x = 10;
    double max_x = view->width() - 30;

    if(text_left <= min_x)
    {
        // Compute the pixels needed to add to have top-left at 0
        x = x - text_left + min_x;
    }
    else if(text_right >= max_x)
    {
        // Compute the pixels needed to add to have top-right at max
        x = x - text_right + max_x;
    }

    x = std::max(x, 10.);
    double y = 2.5;
    double w = m_width - x;
    double h = ConstraintHeader::headerHeight();


    if(std::abs(m_previous_x - x) > 1)
    {
        m_previous_x = x;
    }
    // TODO m_textCache.draw(painter, QPointF{m_previous_x,y}, {}, boundingRect();
    auto font = Skin::instance().SansFont;
    font.setPointSize(10);
    font.setBold(true);
    painter->drawText(m_previous_x,y,w,h, Qt::AlignLeft, m_text);

    if(m_width > 20)
    {
        painter->setPen(ScenarioStyle::instance().ConstraintHeaderBottomLine.getColor());
        painter->drawLine(
                    boundingRect().bottomLeft(),
                    boundingRect().bottomRight());
    }
}

void TemporalConstraintHeader::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  emit doubleClicked();
}

void TemporalConstraintHeader::on_textChange()
{
  auto font = Skin::instance().SansFont;
  font.setPointSize(10);
  font.setBold(true);
  QFontMetrics fm(font);
  m_textWidthCache = fm.width(m_text);


  m_textCache.setFont(font);
  m_textCache.setText(m_text);

  m_textCache.beginLayout();
  QTextLine line = m_textCache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textCache.endLayout();
}
}
