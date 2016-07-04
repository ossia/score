#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QTextLayout>
#include <QRect>

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TemporalConstraintHeader final : public ConstraintHeader
{
        Q_OBJECT
    public:
        TemporalConstraintHeader();

        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
    signals:
        void doubleClicked();

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        void on_textChange() override;
        int m_previous_x{};

        int m_textWidthCache;
        QTextLayout m_textCache;
};
}
