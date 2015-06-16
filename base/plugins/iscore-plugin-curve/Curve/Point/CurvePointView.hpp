#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <QGraphicsItem>
class CurvePointModel;
class CurvePointView : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurvePointView(CurvePointModel* model,
                       QGraphicsItem* parent);

        CurvePointModel& model() const;

        int type() const override;
        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        void setSelected(bool selected);

    signals:
        void contextMenuRequested(const QPoint&);

    protected:
        void contextMenuEvent(QGraphicsSceneContextMenuEvent*) override;


    private:
        CurvePointModel* m_model;
        bool m_selected{};
};
