#pragma once
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <iscore/tools/SettableIdentifier.hpp>

class QGraphicsSceneContextMenuEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
class SegmentModel;

struct Style;
class SegmentView final : public QGraphicsObject
{
        Q_OBJECT
    public:
        SegmentView(
                const SegmentModel* model,
                const Curve::Style& style,
                QGraphicsItem* parent);

        const Id<SegmentModel>& id() const;

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 101; }
        int type() const override
        { return static_type(); }


        QRectF boundingRect() const override;

        QPainterPath shape() const override
        {
            return m_strokedShape;
        }

        QPainterPath opaqueArea() const override
        {
            return m_strokedShape;
        }

        bool contains(const QPointF& pt) const override
        {
            return m_strokedShape.contains(pt);
        }

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        void setModel(const SegmentModel*);
        const SegmentModel& model() const
        { return *m_model; }

        void setRect(const QRectF& theRect);

        void setSelected(bool selected);

        void enable();
        void disable();

    signals:
        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    private:
        void updatePoints();
        // Takes a table of points and draws them in a square given by the boundingRect
        // QGraphicsItem interface
        QRectF m_rect;

        const SegmentModel* m_model{};
        const Curve::Style& m_style;
        bool m_selected{};

        QPainterPath m_unstrokedShape;
        QPainterPath m_strokedShape;

        bool m_enabled{true};
};

}
