#pragma once
#include <QGraphicsObject>

class RackView final : public QGraphicsObject
{
        Q_OBJECT

    public:
        RackView(QGraphicsObject* parent);
        virtual ~RackView() = default;


        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

        void setHeight(int height)
        {
            prepareGeometryChange();
            m_height = height;
        }

        void setWidth(int width)
        {
            prepareGeometryChange();
            m_width = width;
        }

    private:
        int m_height {};
        int m_width {};
};
