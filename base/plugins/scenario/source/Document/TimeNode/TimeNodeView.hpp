#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>

class TimeNodePresenter;
class TimeNodeView : public QGraphicsObject
{
        Q_OBJECT

    public:
        TimeNodeView(const TimeNodePresenter& presenter,
                     QGraphicsObject* parent);
        ~TimeNodeView() = default;

        int type() const override
        { return QGraphicsItem::UserType + 3; }

        const auto& presenter() const
        { return m_presenter;}

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        // QGraphicsItem interface
        QRectF boundingRect() const override;

        void setExtremities(int top, int bottom);

        void setMoving(bool);
        void setSelected(bool selected)
        {
            m_selected = selected;
            update();
        }

        bool isSelected() const
        {
            return m_selected;
        }

    public slots:
        void changeColor(QColor);

    private:
        const TimeNodePresenter& m_presenter;
        int m_top {0};
        int m_bottom {0};

        QPointF m_clickedPoint {};
        QColor m_color;
        bool m_moving {false};
        bool m_selected{};
};
