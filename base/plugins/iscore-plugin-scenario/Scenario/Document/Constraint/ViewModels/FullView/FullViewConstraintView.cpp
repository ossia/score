#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <QtGlobal>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>
#include <QCursor>

#include "FullViewConstraintPresenter.hpp"
#include "FullViewConstraintView.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
FullViewConstraintView::FullViewConstraintView(FullViewConstraintPresenter& presenter,
                                               QGraphicsItem *parent) :
    ConstraintView {presenter, parent}
{
    this->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    this->setParentItem(parent);
    this->setFlag(ItemIsSelectable);

    this->setZValue(1);
    this->setY(2*constraintAndRackHeight());
}

QRectF FullViewConstraintView::boundingRect() const
{
    return {0, 0, qreal(maxWidth()) + 3, qreal(constraintAndRackHeight()) + 3};
}

void FullViewConstraintView::paint(QPainter* painter,
                                   const QStyleOptionGraphicsItem* option,
                                   QWidget* widget)
{
    auto& skin = ScenarioStyle::instance();
    const qreal min_w = minWidth();
    const qreal max_w = maxWidth();
    const qreal def_w = defaultWidth();
    const qreal play_w = playWidth();

    painter->setRenderHint(QPainter::Antialiasing, false);

    QColor c;
    if(isSelected())
    {
        c = skin.ConstraintSelected.getColor();
    }
    else if(parentItem()->isSelected())
    {
        c = skin.ConstraintFullViewParentSelected.getColor();
    }
    else
    {
        c = skin.ConstraintBase.getColor();
    }

    skin.ConstraintSolidPen.setColor(c);

    if(min_w == max_w)
    {
        painter->setPen(skin.ConstraintSolidPen);
        painter->drawLine(0, 0, def_w, 0);
    }
    else
    {
        // First the line going from 0 to the min
        painter->setPen(skin.ConstraintSolidPen);
        painter->drawLine(0, 0, min_w, 0);

        // The little hat
        painter->drawLine(min_w, -5, min_w, -15);
        painter->drawLine(min_w, -15, max_w, -15);
        painter->drawLine(max_w, -5, max_w, -15);

        // Finally the dashed line
        skin.ConstraintDashPen.setColor(c);
        painter->setPen(skin.ConstraintDashPen);
        painter->drawLine(min_w, 0, max_w, 0);
    }

    auto pw = playWidth();
    if(pw != 0.)
    {
        skin.ConstraintPlayPen.setColor(skin.ConstraintPlayFill.getColor());
        painter->setPen(skin.ConstraintPlayPen);
        painter->drawLine(0, 0, std::min(play_w, std::max(def_w, max_w)), 0);
    }
#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::red);
    painter->drawRect(boundingRect());
#endif
}
}
