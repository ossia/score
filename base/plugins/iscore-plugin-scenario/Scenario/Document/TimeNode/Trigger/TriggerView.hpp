#pragma once
#include <QGraphicsItem>
#include <QRect>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

class QGraphicsSceneMouseEvent;
class QGraphicsSvgItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TriggerView final : public QGraphicsObject
{
        Q_OBJECT
    public:
        TriggerView(QGraphicsItem* parent);
        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

        static constexpr int static_type()
        { return QGraphicsItem::UserType + ItemType::Trigger; }
        int type() const override
        { return static_type(); }

    signals:
        void pressed();

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;

    private:
        QGraphicsSvgItem* m_item{};
};
}
