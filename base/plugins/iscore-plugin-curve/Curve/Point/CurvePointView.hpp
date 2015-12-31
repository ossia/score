#pragma once
#include <QGraphicsItem>
#include <QPoint>
#include <QRect>

class QGraphicsSceneContextMenuEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
class PointModel;
struct Style;
class PointView final : public QGraphicsObject
{
        Q_OBJECT
    public:
        PointView(
                const PointModel* model,
                const Curve::Style& style,
                QGraphicsItem* parent);

        const PointModel& model() const;
        const Id<PointModel>& id() const;

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 10; }
        int type() const override
        { return static_type(); }

        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

        void setSelected(bool selected);

        void enable();
        void disable();

        void setModel(const PointModel* model);

    signals:
        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;


    private:
        const PointModel* m_model;
        const Curve::Style& m_style;
        bool m_selected{};
        bool m_enabled{true};
};

}
