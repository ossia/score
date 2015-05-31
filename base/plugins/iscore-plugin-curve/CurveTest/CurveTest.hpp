#pragma once
#include <QGraphicsItem>
#include <utility>
#include <type_traits>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
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

        template<typename Impl>
        CurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<CurveSegmentModel>{vis, parent}
        {
            vis.writeTo(*this);
        }


        virtual QString name() const = 0;
        virtual void serialize(const VisitorVariant&) const = 0;
        virtual QVector<QPointF> data(int numInterp) const = 0; // Will interpolate


        QPointF start() const;
        void setStart(const QPointF& pt)
        {
            m_start = pt;
            on_startChanged();
        }

        QPointF end() const;
        void setEnd(const QPointF& pt)
        {
            m_end = pt;
            on_endChanged();
        }

        const id_type<CurveSegmentModel>& previous() const;
        void setPrevious(const id_type<CurveSegmentModel>& previous);

        const id_type<CurveSegmentModel>& following() const;
        void setFollowing(const id_type<CurveSegmentModel>& following);


    signals:
        void dataChanged();
        void previousChanged();
        void followingChanged();

    protected:
        virtual void on_startChanged() = 0;
        virtual void on_endChanged() = 0;

    private:
        QPointF m_start, m_end;
        id_type<CurveSegmentModel> m_previous, m_following;
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

        void clear()
        {
            qDeleteAll(m_segments);
            m_segments.clear();
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
};


// ALl between zero and one.
class CurveSegmentLinearModel : public CurveSegmentModel
{
    public:
        using CurveSegmentModel::CurveSegmentModel;


        // TODO Factor this in a macro.
        template<typename Impl>
        CurveSegmentLinearModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        QString name() const override
        {
            return "Linear";
        }

        void serialize(const VisitorVariant& vis) const
        {
            serialize_dyn(vis, *this);
        }

        void on_startChanged() override
        {
            emit dataChanged();
        }

        void on_endChanged() override
        {
            emit dataChanged();
        }

        virtual QVector<QPointF> data(int numInterp) const override
        {
            QVector<QPointF> interppts;
            interppts.resize(numInterp + 1);

            for(int j = 0; j <= numInterp; j++)
            {
                interppts[j] = start() + double(j) / numInterp * (end() - start());
            }

            return interppts;
        }
};
