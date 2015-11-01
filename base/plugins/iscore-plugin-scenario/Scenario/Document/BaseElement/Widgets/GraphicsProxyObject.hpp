#pragma once
#include <QGraphicsObject>
#include <QDebug>
#include <QBrush>
#include <QPainter>
class GraphicsProxyObject : public QGraphicsItem
{
    public:
        using QGraphicsItem::QGraphicsItem;
        QRectF boundingRect() const override
        {
            return {};
        }

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
        {
        }
};
