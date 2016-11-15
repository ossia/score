#include <Process/Style/ProcessFonts.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <qnamespace.h>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <iscore/model/Skin.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include "TemporalConstraintPresenter.hpp"
#include "TemporalConstraintView.hpp"

class QGraphicsSceneHoverEvent;
class QStyleOptionGraphicsItem;
class QWidget;


namespace Scenario
{
TemporalConstraintView::TemporalConstraintView(
        TemporalConstraintPresenter &presenter,
        QGraphicsItem* parent) :
    ConstraintView {presenter, parent},
    m_bgColor{ScenarioStyle::instance().ConstraintDefaultBackground},
    m_labelItem{new SimpleTextItem{this}},
    m_counterItem{new SimpleTextItem{this}}
{
    this->setCacheMode(QGraphicsItem::NoCache);
    this->setParentItem(parent);
    this->setAcceptDrops(true);

    this->setZValue(ZPos::Constraint);

    const int fontSize = 12;
    auto f = iscore::Skin::instance().SansFont;
    f.setBold(false);
    f.setPointSize(fontSize);
    f.setStyleStrategy(QFont::NoAntialias);
    m_labelItem->setFont(f);
    m_labelItem->setPos(0, -16);
    m_labelItem->setAcceptedMouseButtons(Qt::MouseButton::NoButton);
    m_labelItem->setAcceptHoverEvents(false);
    f.setPointSize(7);
    f.setStyleStrategy(QFont::NoAntialias);
    f.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
    m_counterItem->setFont(f);
    m_counterItem->setColor(iscore::ColorRef(&iscore::Skin::Light));
    m_counterItem->setAcceptedMouseButtons(Qt::MouseButton::NoButton);
    m_counterItem->setAcceptHoverEvents(false);
}



void TemporalConstraintView::paint(
        QPainter* p,
        const QStyleOptionGraphicsItem*,
        QWidget*)
{
    QPainterPath solidPath, dashedPath, playedPath;
    auto& painter = *p;
    painter.setRenderHint(QPainter::Antialiasing, false);
    auto& skin = ScenarioStyle::instance();

    qreal min_w = minWidth();
    qreal max_w = maxWidth();
    qreal def_w = defaultWidth();
    qreal play_w = playWidth();

    m_labelItem->setPos(def_w / 2. - m_labelItem->boundingRect().width() / 2., -17);
    m_counterItem->setPos(def_w - m_counterItem->boundingRect().width() - 5, 5);

    // Draw the stuff present if there is a rack *in the model* ?
    if(presenter().rack())
    {
        // Background
        auto rect = boundingRect();
        rect.adjust(0,4,0,-10);
        rect.setWidth(this->defaultWidth());

        QColor bgColor = m_bgColor.getColor();
        bgColor.setAlpha(m_hasFocus ? 84 : 76);
        painter.fillRect(rect, bgColor);

        // Fake timenode continuation
        skin.ConstraintRackPen.setColor(skin.RackSideBorder.getColor());
        painter.setPen(skin.ConstraintRackPen);
        painter.drawLine(rect.topLeft(), rect.bottomLeft());
        painter.drawLine(rect.topRight(), rect.bottomRight());
    }



    // Paths
    if(infinite())
    {
        if(min_w != 0.)
        {
            solidPath.lineTo(min_w, 0);
        }

        // TODO end state should be hidden
        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(def_w, 0);
    }
    else if(min_w == max_w) // TODO rigid()
    {
        solidPath.lineTo(def_w, 0);
    }
    else
    {
        if(min_w != 0.)
        {
            solidPath.lineTo(min_w, 0);
        }

        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(max_w, 0);
    }

    if(play_w != 0.)
    {
        playedPath.lineTo(std::min(play_w, std::max(def_w, max_w)), 0);
    }

    // Colors
    QColor constraintColor;
    // TODO make a switch instead
    if(isSelected())
    {
        constraintColor = skin.ConstraintSelected.getColor();
    }
    else if(warning())
    {
        constraintColor = skin.ConstraintWarning.getColor();
    }
    else
    {
        constraintColor = skin.ConstraintBase.getColor();
    }
    if(! isValid() || m_state == ConstraintExecutionState::Disabled)
    {
        constraintColor = skin.ConstraintInvalid.getColor();
    }

    if(m_shadow)
        constraintColor = constraintColor.lighter();


    skin.ConstraintSolidPen.setColor(constraintColor);
    skin.ConstraintDashPen.setColor(constraintColor);

    // Drawing
    if(!solidPath.isEmpty())
    {
        painter.setPen(skin.ConstraintSolidPen);
        painter.drawPath(solidPath);
    }

    if(!dashedPath.isEmpty())
    {
        painter.setPen(skin.ConstraintDashPen);
        painter.drawPath(dashedPath);
    }

    if(!playedPath.isEmpty())
    {
        skin.ConstraintPlayPen.setColor(skin.ConstraintPlayFill.getColor());
        painter.setPen(skin.ConstraintPlayPen);
        painter.drawPath(playedPath);
    }

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter.setPen(Qt::darkRed);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(boundingRect());
#endif
}

void TemporalConstraintView::hoverEnterEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsItem::hoverEnterEvent(h);
    m_shadow = true;
    update();
    emit constraintHoverEnter();
}

void TemporalConstraintView::hoverLeaveEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsItem::hoverLeaveEvent(h);
    m_shadow = false;
    update();
    emit constraintHoverLeave();
}

void TemporalConstraintView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    QGraphicsItem::dragEnterEvent(event);
    m_shadow = true;
    update();
    event->accept();

}

void TemporalConstraintView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
    QGraphicsItem::dragLeaveEvent(event);
    m_shadow = false;
    update();
    event->accept();
}

void TemporalConstraintView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    emit dropReceived(event->pos(), event->mimeData());

    event->accept();
}

void TemporalConstraintView::setLabel(const QString &label)
{
    m_labelItem->setText(label);
    update();
}

void TemporalConstraintView::setExecutionState(ConstraintExecutionState s)
{
    m_state = s;
    update();
}

void TemporalConstraintView::setExecutionDuration(const TimeValue& progress)
{
    // FIXME this should be merged with the slot in ConstraintPresenter!!!
    // Also make a setting to disable it since it may take a lot of time
    if(!qFuzzyCompare(progress.msec(), 0))
    {
        m_counterItem->setVisible(true);
        m_counterItem->setText(progress.toString());
    }
    else
    {
        m_counterItem->setVisible(false);
    }
    update();
}

void TemporalConstraintView::setLabelColor(iscore::ColorRef labelColor)
{
    m_labelItem->setColor(labelColor);
    update();
}

bool TemporalConstraintView::shadow() const
{
    return m_shadow;
}

void TemporalConstraintView::setShadow(bool shadow)
{
    m_shadow = shadow;
    update();
}
}
