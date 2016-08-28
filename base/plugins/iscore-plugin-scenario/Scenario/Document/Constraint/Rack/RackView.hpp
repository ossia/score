#pragma once

#include <QGraphicsItem>
#include <QRect>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class RackView final :
        public QObject,
        public QGraphicsItem
{
        Q_OBJECT
        Q_INTERFACES(QGraphicsItem)

    public:
        RackView(QGraphicsItem* parent);
        virtual ~RackView() = default;


        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

        void setHeight(qreal height)
        {
            prepareGeometryChange();
            m_height = height;
        }

        void setWidth(qreal width)
        {
            prepareGeometryChange();
            m_width = width;
        }

    private:
        qreal m_height {};
        qreal m_width {};
};
}
