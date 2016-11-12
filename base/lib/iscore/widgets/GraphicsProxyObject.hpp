#pragma once
#include <QGraphicsItem>
#include <QObject>
#include <QDebug>
#include <QBrush>
#include <QPainter>
class BaseGraphicsObject final :
    public QObject,
    public QGraphicsItem
{
    public:
        BaseGraphicsObject(QGraphicsItem* parent = nullptr):
            QGraphicsItem{parent}
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
