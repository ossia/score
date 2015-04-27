#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"

class TemporalConstraintPresenter;
class TemporalConstraintView : public AbstractConstraintView
{
        Q_OBJECT

    public:
        TemporalConstraintView(TemporalConstraintPresenter& presenter,
                               QGraphicsObject* parent);

        virtual ~TemporalConstraintView() = default;

        virtual QRectF boundingRect() const override;
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

        bool shadow() const;
        void setShadow(bool shadow);

    signals:
        void constraintHoverEnter();
        void constraintHoverLeave();

    protected:
        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

    private:
        QPointF m_clickedPoint {};

        bool m_shadow {false};
};
