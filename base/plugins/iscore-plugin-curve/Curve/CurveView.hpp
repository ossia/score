#pragma once
#include <QGraphicsItem>

class CurveView : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurveView(QGraphicsItem* parent);
        virtual ~CurveView();

        void setRect(const QRectF& theRect);

        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

        void setSelectionArea(const QRectF&);
    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void escPressed();

        void keyPressed(int);
        void keyReleased(int);

        void contextMenuRequested(const QPoint&);

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

