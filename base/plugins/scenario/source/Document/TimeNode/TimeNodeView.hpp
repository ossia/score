#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>


class TimeNodeView : public QGraphicsObject
{
    public:
        TimeNodeView(QGraphicsObject* parent);
        virtual ~TimeNodeView() = default;

        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);
    private:
        int m_top{0};
        int m_bottom{0};
};
