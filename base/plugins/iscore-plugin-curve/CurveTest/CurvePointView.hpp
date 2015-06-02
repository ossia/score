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
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        CurvePointModel* m_model;
        bool m_selected{};
};
