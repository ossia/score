#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"

class FullViewConstraintViewModel;
class FullViewConstraintPresenter;

class FullViewConstraintView : public AbstractConstraintView
{
        Q_OBJECT

    public:
        FullViewConstraintView(FullViewConstraintPresenter &presenter,
                               QGraphicsItem* parent);

        virtual ~FullViewConstraintView() = default;

        virtual QRectF boundingRect() const override;
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;
};
