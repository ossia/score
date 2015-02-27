#pragma once
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QPointF>
#include <QPen>

class AbstractConstraintViewModel;

class AbstractConstraintView : public QGraphicsObject
{
        Q_OBJECT

    public:
        AbstractConstraintView(QGraphicsObject* parent);

        virtual ~AbstractConstraintView() = default;

        void setDefaultWidth(int width);
        void setMaxWidth(int max);
        void setMinWidth(int min);
        void setHeight(int height);

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
        //void constraintPressed(QPointF);
        void constraintSelectionChanged(bool);

    protected:
        virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

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
};
