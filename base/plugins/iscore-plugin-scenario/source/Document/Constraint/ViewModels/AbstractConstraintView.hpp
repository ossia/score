#pragma once
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QPointF>
#include <QPen>
#include <ProcessInterface/TimeValue.hpp>

class AbstractConstraintViewModel;
class AbstractConstraintPresenter;
class AbstractConstraintView : public QGraphicsObject
{
        Q_OBJECT

    public:
        AbstractConstraintView(AbstractConstraintPresenter& presenter,
                               QGraphicsObject* parent);

        int type() const override
        { return QGraphicsItem::UserType + 2; }

        virtual ~AbstractConstraintView() = default;

        const auto& presenter() const
        { return m_presenter;}


        virtual void setInfinite(bool);
        bool infinite() const
        { return m_infinite; }

        void setDefaultWidth(double width);
        void setMaxWidth(bool infinite, double max);
        void setMinWidth(double min);
        void setHeight(double height);
        void setPlayWidth(double width);

        double height() const
        { return m_height; }


        void setSelected(bool selected)
        {
            m_selected = selected;
            update();
        }

        bool isSelected() const
        {
            return m_selected;
        }


        double defaultWidth() const
        {
            return m_defaultWidth;
        }

        double minWidth() const
        {
            return m_minWidth;
        }

        double maxWidth() const
        {
            return m_maxWidth;
        }

        double constraintHeight() const
        {
            return m_height;
        }

        double playWidth() const
        {
            return m_playWidth;
        }

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

        QPen m_solidPen
        {
            QBrush{Qt::black},
            2,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin
        };
        QPen m_dashPen
        {
            QBrush{Qt::black},
            2,
            Qt::DashLine,
            Qt::RoundCap,
            Qt::RoundJoin
        };

    private:
        AbstractConstraintPresenter& m_presenter;
        double m_defaultWidth {};
        double m_maxWidth {};
        double m_minWidth {};
        double m_playWidth {};

        double m_height {};

        bool m_selected{};
        bool m_infinite{};
};
