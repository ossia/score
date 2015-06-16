#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include <QPainter>
class TemporalConstraintPresenter;

class TemporalConstraintView : public AbstractConstraintView
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

    signals:
        void constraintHoverEnter();
        void constraintHoverLeave();

    protected:
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

    private:
        QPointF m_clickedPoint {};

        bool m_shadow {false};
        QString m_label{};
        QColor m_labelColor{Qt::gray};

};
