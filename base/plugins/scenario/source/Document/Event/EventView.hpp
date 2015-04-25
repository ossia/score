#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>
#include <QKeyEvent>
class EventPresenter;
class EventView : public QGraphicsObject
{
        Q_OBJECT

    public:
        EventView(const EventPresenter& presenter, QGraphicsObject* parent);
        virtual ~EventView() = default;

        const auto& presenter() const
        { return m_presenter;}

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setSelected(bool selected)
        {
            m_selected = selected;
            update();
        }

        bool isSelected() const
        {
            return m_selected;
        }

        bool isShadow() const
        {
            return m_shadow;
        }

    signals:
        void eventHoverEnter();
        void eventHoverLeave();

    public slots:
        void changeColor(QColor);
        void setMoving(bool arg);
        void setShadow(bool arg);

    protected:
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

    private:
        const EventPresenter& m_presenter;
        QPointF m_clickedPoint {};
        QColor m_color;

        bool m_moving {false};
        bool m_shadow {false};
        bool m_selected{};
};

