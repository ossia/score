#pragma once
#include <QGraphicsObject>

class Layer : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;

        virtual QRectF boundingRect() const override;

        void setHeight(qreal height);
        qreal height() const;

        void setWidth(qreal width);
        qreal width() const;

    private:
        qreal m_height {};
        qreal m_width {};
};
