#pragma once
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QPointF>
#include <QPen>
#include <ProcessInterface/TimeValue.hpp>

class AbstractConstraintViewModel;

class AbstractConstraintView : public QGraphicsObject
{
        Q_OBJECT

    public:
        AbstractConstraintView(QGraphicsObject* parent);

        virtual ~AbstractConstraintView() = default;

        virtual void setInfinite(bool);
        bool infinite() const
        { return m_infinite; }

        void setDefaultWidth(double width);
        void setMaxWidth(bool infinite, double max);
        void setMinWidth(double min);
        void setHeight(double height);
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

    signals:
        void constraintPressed();

    protected:
        QPen m_solidPen
        {
            QBrush{Qt::black},
            4,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin
        };
        QPen m_dashPen
        {
            QBrush{Qt::black},
            4,
            Qt::DashLine,
            Qt::RoundCap,
            Qt::RoundJoin
        };

    private:
        double m_defaultWidth {};
        double m_maxWidth {};
        double m_minWidth {};

        double m_height {};

        bool m_selected{};
        bool m_infinite{};
};
