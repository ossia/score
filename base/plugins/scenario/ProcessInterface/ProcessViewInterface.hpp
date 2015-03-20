#pragma once
#include <QGraphicsObject>

class ProcessViewInterface : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;

        virtual QRectF boundingRect() const override
        {
            return {0, 0, m_width, m_height};
        }

        void setHeight(qreal height)
        {
            prepareGeometryChange();
            m_height = height;
        }

        qreal height() const
        {
            return m_height;
        }

        void setWidth(qreal width)
        {
            prepareGeometryChange();
            m_width = width;
        }

        qreal width() const
        {
            return m_width;
        }

    private:
        qreal m_height {};
        qreal m_width {};
};
