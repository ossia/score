#pragma once
#include <QGraphicsObject>

class GraphicsProxyObject : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;
        virtual QRectF boundingRect() const
        {
            return QRectF {};
        }

        virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
        {
        }
};
