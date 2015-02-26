#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"

class FullViewConstraintViewModel;

class FullViewConstraintView : public AbstractConstraintView
{
        Q_OBJECT

    public:
        FullViewConstraintView(QGraphicsObject* parent);

        virtual ~FullViewConstraintView() = default;

        virtual QRectF boundingRect() const override;
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent* m) override;

    private:
        QPointF m_clickedPoint {};

        FullViewConstraintViewModel* m_viewModel {};
};
