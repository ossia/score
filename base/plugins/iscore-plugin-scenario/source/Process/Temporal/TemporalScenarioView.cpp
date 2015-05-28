#include "TemporalScenarioView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>

#include "TemporalScenarioPresenter.hpp"

TemporalScenarioView::TemporalScenarioView(QGraphicsObject* parent) :
    ProcessView {parent}
{
    this->setParentItem(parent);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
    this->setCursor(Qt::ArrowCursor);

    this->setZValue(parent->zValue() + 1);

    m_clearAction = new QAction{tr("Clear content"), this};
    connect(m_clearAction,  &QAction::triggered,
            this,           &TemporalScenarioView::clearPressed);
}

TemporalScenarioView::~TemporalScenarioView()
{
    delete m_clearAction;
}

void TemporalScenarioView::paint(QPainter* painter,
                                 const QStyleOptionGraphicsItem* option,
                                 QWidget* widget)
{
    if(m_lock)
    {
        painter->setBrush({Qt::red, Qt::DiagCrossPattern});
        painter->drawRect(boundingRect());
    }

    if(m_selectArea != QRectF{})
    {
        painter->setCompositionMode(QPainter::CompositionMode_Xor);
        painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});

        painter->drawRect(m_selectArea);
    }
}


void TemporalScenarioView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
        emit scenarioPressed(event->scenePos());
}

void TemporalScenarioView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit scenarioMoved(event->scenePos());
}

void TemporalScenarioView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit scenarioReleased(event->scenePos());
}

void TemporalScenarioView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu contextMenu {};
    contextMenu.clear();

    contextMenu.addAction(m_clearAction);

    contextMenu.exec(event->screenPos());
    //TODO contextMenu : doit renvoyer juste la position au presenter
}

void TemporalScenarioView::keyPressEvent(QKeyEvent* event)
{
    QGraphicsObject::keyPressEvent(event);
    if(event->key() == Qt::Key_Escape)
    {
        emit escPressed();
    }
    if(event->key() == Qt::Key_Shift)
    {
        emit shiftPressed();
    }
}

void TemporalScenarioView::keyReleaseEvent(QKeyEvent *event)
{
    QGraphicsObject::keyReleaseEvent(event);
    if(event->key() == Qt::Key_Shift)
    {
        emit shiftReleased();
    }
}
