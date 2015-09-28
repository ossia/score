#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>
#include "Document/VerticalExtent.hpp"

class TimeNodePresenter;
class TimeNodeView : public QGraphicsObject
{
        Q_OBJECT

    public:
        TimeNodeView(TimeNodePresenter& presenter,
                     QGraphicsObject* parent);
        ~TimeNodeView() = default;

        int type() const override final
        { return QGraphicsItem::UserType + 3; }

        const TimeNodePresenter& presenter() const
        { return m_presenter;}

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        // QGraphicsItem interface
        QRectF boundingRect() const override final
        { return { -3., 0., 6., m_extent.bottom() - m_extent.top()}; }

        void setExtent(const VerticalExtent& extent);
        void setExtent(VerticalExtent&& extent);
        void addPoint(int newY);

        void setMoving(bool);
        void setSelected(bool selected);

        bool isSelected() const
        {
            return m_selected;
        }

    public slots:
        void changeColor(QColor);

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        TimeNodePresenter& m_presenter;
        VerticalExtent m_extent;

        QPointF m_clickedPoint {};
        QColor m_color;
        bool m_selected{};
};
