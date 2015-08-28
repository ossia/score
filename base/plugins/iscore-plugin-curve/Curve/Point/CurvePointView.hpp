#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <QGraphicsItem>
class CurvePointModel;
class CurvePointView : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurvePointView(const CurvePointModel& model,
                       QGraphicsItem* parent);

        const CurvePointModel& model() const;
        const Id<CurvePointModel>& id() const;

        int type() const override;
        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        void setSelected(bool selected);

        void enable();
        void disable();

    signals:
        void contextMenuRequested(const QPoint&);

    protected:
        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;


    private:
        const CurvePointModel& m_model;
        bool m_selected{};
        bool m_enabled{true};
};
