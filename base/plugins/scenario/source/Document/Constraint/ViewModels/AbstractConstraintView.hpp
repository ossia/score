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

        void setDefaultWidth(int width);
        void setMaxWidth(bool infinite, int max);
        void setMinWidth(int min);
        void setHeight(int height);


        void setSelected(bool selected)
        {
            m_selected = selected;
            update();
        }

        bool isSelected() const
        {
            return m_selected;
        }


        int defaultWidth() const
        {
            return m_defaultWidth;
        }

        int minWidth() const
        {
            return m_minWidth;
        }

        int maxWidth() const
        {
            return m_maxWidth;
        }

        int constraintHeight() const
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
        int m_defaultWidth {};
        int m_maxWidth {};
        int m_minWidth {};

        int m_height {};

        bool m_selected{};
        bool m_infinite{};
};
