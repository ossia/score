#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>

class AddressBarItem;
class FullViewConstraintHeader final : public ConstraintHeader
{
    public:
        FullViewConstraintHeader(QGraphicsItem*);

        AddressBarItem* bar() const;

        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    private:
        AddressBarItem* m_bar{};
};
