#pragma once

#include <QtGlobal>
#include <QGraphicsItem>
#include <QRect>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class LayerView : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;
        virtual ~LayerView();

        QRectF boundingRect() const final override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) final override;

        void setHeight(qreal height);
        qreal height() const;

        void setWidth(qreal width);
        qreal width() const;

    protected:
        virtual void paint_impl(QPainter*) const = 0;

    private:
        qreal m_height {};
        qreal m_width {};
};
