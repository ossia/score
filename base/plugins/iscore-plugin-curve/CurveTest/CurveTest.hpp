#pragma once
#include <QGraphicsItem>
#include <utility>
#include <type_traits>
#include <iscore/tools/IdentifiedObject.hpp>
//namespace {
class MyPoint;
class PointsLayer;
//}

class CurveSegmentModel;
class CurveSegmentView : public QGraphicsItem
{
        // Takes a table of points and draws them in a square given by the boundingRect
        // QGraphicsItem interface
        QVector<QPointF> points; // each between rect.topLeft() :: rect.bottomRight()
        QRectF rect;

    public:
        CurveSegmentView(CurveSegmentModel* model, QGraphicsItem* parent);

        void setRect(const QRectF& theRect)
        {
            prepareGeometryChange();
            rect = theRect;
        }

        void setPoints(QVector<QPointF>&& thePoints)
        {
            points = std::move(thePoints);
            update();
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};

// Gives the data.
class CurveSegmentModel : public IdentifiedObject<CurveSegmentModel>
{
        Q_OBJECT
    public:
        CurveSegmentModel(const id_type<CurveSegmentModel>& id, QObject* parent):
            IdentifiedObject<CurveSegmentModel>{id, "CurveSegmentModel", parent}

        {

        }

        virtual void setStart(const QPointF& pt) = 0;
        virtual void setStop(const QPointF& pt) = 0;

        virtual QVector<QPointF> data(int numInterp) const = 0; // Will interpolate

        CurveSegmentModel* previous() const;
        void setPrevious(CurveSegmentModel *previous);

        CurveSegmentModel* following() const;
        void setFollowing(CurveSegmentModel *following);

    signals:
        void dataChanged();
        void previousChanged();
        void followingChanged();

    private:
        CurveSegmentModel* m_previous{};
        CurveSegmentModel* m_following{};
};

class CurveModel : public QObject
{
        QVector<CurveSegmentModel*> m_segments; // Each between 0, 1

        Q_OBJECT
    public:
        void addSegment(CurveSegmentModel* m)
        {
            m_segments.append(m);
            emit segmentAdded(m);
        }

        void removeSegment(CurveSegmentModel* m)
        {
            auto index = m_segments.indexOf(m);
            if(index >= 0)
            {
                m_segments.remove(index);
            }

            emit segmentRemoved(m);
        }

        const QVector<CurveSegmentModel*>& segments() const
        {
            return m_segments;
        }

    signals:
        void segmentAdded(CurveSegmentModel*);
        void segmentRemoved(CurveSegmentModel*);
};

class CurveView : public QGraphicsItem
{
    QRectF rect; // The rect in which the whole curve must fit.

    friend class ::MyPoint;
    public:
    using QGraphicsItem::QGraphicsItem;
        void setRect(const QRectF& theRect)
        {
            prepareGeometryChange();
            rect = theRect;
            updateSubitems();
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

        void updateSubitems();

    signals:
        // Ne raisonner qu'à partir des segments dans le modèle ?
        // La vue crée les points en plus.
        // Donner xmin, xmax, et équation (type) du segment.

        /*
        void setCurrentPointPos(const QPointF &point);
        void removeFakePoint();
        void pointMoved(const QPointF& pt);
        QPointF pointUnderMouse(QGraphicsSceneMouseEvent *event);
    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event);
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event);


        QSizeF m_size;
        QPointF m_backedUpPoint{-1, -1};

        //QPen m_pen;
        PointsLayer* m_pointLayer{};
        MyPoint* m_fakePoint{};
        */
};


// ALl between zero and one.
class CurveSegmentLinearModel : public CurveSegmentModel
{
    public:
        using CurveSegmentModel::CurveSegmentModel;
        QPointF start, end;
        void setStart(const QPointF& pt) override
        {
            start = pt;
            emit dataChanged();
        }

        void setStop(const QPointF& pt) override
        {
            end = pt;
            emit dataChanged();
        }

        virtual QVector<QPointF> data(int numInterp) const override
        {
            QVector<QPointF> interppts;
            interppts.resize(numInterp + 1);

            for(int j = 0; j <= numInterp; j++)
            {
                interppts[j] = start + double(j) / numInterp * (end - start);
            }

            return interppts;
        }
};
