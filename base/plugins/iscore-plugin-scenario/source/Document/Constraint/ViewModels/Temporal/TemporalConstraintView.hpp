#pragma once
#include "Document/Constraint/ViewModels/ConstraintView.hpp"
#include <QPainter>
class TemporalConstraintPresenter;

class TemporalConstraintView : public ConstraintView
{
        Q_OBJECT

    public:
        TemporalConstraintView(TemporalConstraintPresenter& presenter,
                               QGraphicsObject* parent);

        virtual QRectF boundingRect() const override;
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

        bool shadow() const;
        void setShadow(bool shadow);

        void setLabelColor(const QColor &labelColor);
        void setLabel(const QString &label);

        void setFocused(bool b)
        {
            m_hasFocus = b;
            m_bgColor.setAlpha(m_hasFocus ? 84 : 76);
            update();
        }

        void setColor(QColor c)
        {
            m_bgColor = c;
            m_bgColor.setAlpha(m_hasFocus ? 84 : 76);
            update();
        }

    signals:
        void constraintHoverEnter();
        void constraintHoverLeave();

    protected:
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

    private:
        QPointF m_clickedPoint {};

        bool m_shadow {false};
        bool m_hasFocus{};
        QString m_label{};
        QColor m_labelColor{Qt::gray};
        QColor m_bgColor{QColor::fromRgba(qRgba(0, 127, 229, 76))};
};
