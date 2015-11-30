#pragma once
#include <QGraphicsObject>
#include <QDebug>
#include <QBrush>
#include <QPainter>
class BaseGraphicsObject final : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;
        QRectF boundingRect() const override
        {
            return {};
        }

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
        {
        }

        void setSelectionArea(const QRectF&)
        {

        }
};
