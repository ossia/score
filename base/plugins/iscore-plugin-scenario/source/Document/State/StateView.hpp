#pragma once
#include <QGraphicsObject>

class StateView : public QGraphicsObject
{
        Q_OBJECT
    public:
        StateView(QGraphicsObject *parent = 0);
        virtual ~StateView() = default;
        int type() const override;

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
        void setContainMessage(bool);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

    public slots:

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

        bool m_containMessage{false};

};
