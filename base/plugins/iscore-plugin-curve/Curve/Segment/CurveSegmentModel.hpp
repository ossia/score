#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>
#include <Curve/StateMachine/CurvePoint.hpp>
class CurveModel;

// Gives the data.
class CurveSegmentModel : public IdentifiedObject<CurveSegmentModel>
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(CurveSegmentModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(CurveSegmentModel, JSONObject)
    public:
        Selectable selection;
        CurveSegmentModel(
                const Id<CurveSegmentModel>& id,
                QObject* parent);

        template<typename Impl>
        CurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual CurveSegmentModel* clone(
                const Id<CurveSegmentModel>& id,
                QObject* parent) const = 0;

        virtual ~CurveSegmentModel();


        virtual QString name() const = 0;
        virtual void serialize(const VisitorVariant&) const = 0;
        virtual void updateData(int numInterp) const = 0; // Will interpolate.
        virtual double valueAt(double x) const = 0;

        const QVector<QPointF>& data() const
        { return m_data; }


        void setStart(const CurvePoint& pt);
        CurvePoint start() const
        {
            return m_start;
        }

        void setEnd(const CurvePoint& pt);
        CurvePoint end() const
        {
            return m_end;
        }

        void setPrevious(const Id<CurveSegmentModel>& previous);
        const Id<CurveSegmentModel>& previous() const
        {
            return m_previous;
        }

        void setFollowing(const Id<CurveSegmentModel>& following);
        const Id<CurveSegmentModel>& following() const
        {
            return m_following;
        }

        // Between -1 and 1, to map to the real parameter.
        virtual void setVerticalParameter(double p);
        virtual void setHorizontalParameter(double p);
        virtual boost::optional<double> verticalParameter() const;
        virtual boost::optional<double> horizontalParameter() const;

    signals:
        void dataChanged();
        void previousChanged();
        void followingChanged();

    protected:
        virtual void on_startChanged() = 0;
        virtual void on_endChanged() = 0;

        mutable QVector<QPointF> m_data; // A data cache.
        mutable bool m_valid{}; // Used to perform caching.
        // TODO it seems that m_valid is never true.

        CurvePoint m_start, m_end;

    private:
        Id<CurveSegmentModel> m_previous, m_following;
};
