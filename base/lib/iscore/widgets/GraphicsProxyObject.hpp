#pragma once
#include <QGraphicsObject>
#include <QDebug>
#include <QBrush>
#include <QPainter>
class BaseGraphicsObject final : public QGraphicsObject
{
    public:
        BaseGraphicsObject(QGraphicsObject* parent = nullptr):
            QGraphicsObject{parent}
        {
            this->setFlag(QGraphicsItem::ItemHasNoContents, true);
        }

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
