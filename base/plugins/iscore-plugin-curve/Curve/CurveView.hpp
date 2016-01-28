#pragma once

#include <QGraphicsItem>
#include <QPoint>
#include <QRect>
#include <iscore_plugin_curve_export.h>

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
class ISCORE_PLUGIN_CURVE_EXPORT View final : public QGraphicsObject
{
        Q_OBJECT
    public:
        explicit View(QGraphicsItem* parent);
        virtual ~View();

        void setRect(const QRectF& theRect);

        QRectF boundingRect() const override
        { return m_rect; }

        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

        void setSelectionArea(const QRectF&);

    signals:
        void pressed(QPointF);
        void moved(QPointF);
        void released(QPointF);

        void escPressed();

        void keyPressed(int);
        void keyReleased(int);

        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

        void keyPressEvent(QKeyEvent* ev) override;
        void keyReleaseEvent(QKeyEvent* ev) override;

        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;

    private:
        QRectF m_rect; // The rect in which the whole curve must fit.
        QRectF m_selectArea;
};
}

