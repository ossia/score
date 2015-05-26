#pragma once
#include <QGraphicsObject>
#include <QDebug>
#include <QBrush>
#include <QPainter>
class GraphicsProxyObject : public QGraphicsItem
{
    public:
        using QGraphicsItem::QGraphicsItem;
        virtual QRectF boundingRect() const
        {
            return {};
        }

        virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
        {
        }
};
