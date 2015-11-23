#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <QGraphicsItem>
class CurvePointModel;
namespace Curve
{
struct Style;
}

class CurvePointView final : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurvePointView(
                const CurvePointModel* model,
                const Curve::Style& style,
                QGraphicsItem* parent);

        const CurvePointModel& model() const;
        const Id<CurvePointModel>& id() const;

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

        void setModel(const CurvePointModel* model);

    signals:
        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;


    private:
        const CurvePointModel* m_model;
        const Curve::Style& m_style;
        bool m_selected{};
        bool m_enabled{true};
};
