#pragma once
#include <QGraphicsItem>
#include <iscore/tools/SettableIdentifier.hpp>
class CurveSegmentModel;
struct CurveStyle;
class CurveSegmentView final : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurveSegmentView(
                const CurveSegmentModel* model,
                const CurveStyle& style,
                QGraphicsItem* parent);

        const Id<CurveSegmentModel>& id() const;
        int type() const override;
        QRectF boundingRect() const override;
        QPainterPath shape() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        void setModel(const CurveSegmentModel*);
        const CurveSegmentModel& model() const
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

        const CurveSegmentModel* m_model{};
        const CurveStyle& m_style;
        bool m_selected{};

        QPainterPath m_unstrockedShape;

        bool m_enabled{true};
};
