#pragma once
#include <QGraphicsObject>
#include <QFont>

class ConstraintView;
class ConstraintHeader : public QGraphicsObject
{
    public:
        enum class State {
            Hidden, // No rack, we show nothing
            RackHidden, // There is at least a hidden rack in the constraint
            RackShown // There is a rack currently shown
        };

        using QGraphicsObject::QGraphicsObject;

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 6; }
        int type() const override
        { return static_type(); }

        void setConstraintView(ConstraintView* view)
        {
            m_view = view;
        }

        static constexpr int headerHeight() { return 20; }

        void setWidth(double width);
        void setText(const QString &text);

        void setState(State s)
        {
            if(s == m_state)
                return;

            if(m_state == State::Hidden)
                show();
            else if(s == State::Hidden)
                hide();

            m_state = s;
            update();
        }

     protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) final override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) final override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) final override;

    protected:
        ConstraintView* m_view{};
        State m_state{};
        double m_width{};
        QString m_text;
};
