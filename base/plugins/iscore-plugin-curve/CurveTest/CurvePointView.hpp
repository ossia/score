#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <QGraphicsItem>
class CurveSegmentModel;
class CurvePointView : public QGraphicsObject
{
        Q_OBJECT
    public:
        CurvePointView(QGraphicsItem* parent);

        int type() const;
        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        const id_type<CurveSegmentModel>& previous() const;
        void setPrevious(const id_type<CurveSegmentModel> &previous);

        const id_type<CurveSegmentModel> &following() const;
        void setFollowing(const id_type<CurveSegmentModel> &following);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        id_type<CurveSegmentModel> m_previous, m_following;
};
