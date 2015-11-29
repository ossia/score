#pragma once
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <QPoint>
#include <QVariant>
#include <vector>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class DataStream;
class JSONObject;
class QObject;
class PowerCurveSegmentModel;

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
        CurveSegmentModel(
                const CurveSegmentData& id,
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


        virtual const CurveSegmentFactoryKey& key() const = 0;
        virtual void serialize(const VisitorVariant&) const = 0;
        virtual void updateData(int numInterp) const = 0; // Will interpolate.
        virtual double valueAt(double x) const = 0;

        const std::vector<QPointF>& data() const
        { return m_data; }


        void setStart(const Curve::Point& pt);
        Curve::Point start() const
        { return m_start; }

        void setEnd(const Curve::Point& pt);
        Curve::Point end() const
        { return m_end; }

        void setPrevious(const Id<CurveSegmentModel>& previous);
        const Id<CurveSegmentModel>& previous() const
        { return m_previous; }

        void setFollowing(const Id<CurveSegmentModel>& following);
        const Id<CurveSegmentModel>& following() const
        { return m_following; }

        // Between -1 and 1, to map to the real parameter.
        virtual void setVerticalParameter(double p);
        virtual void setHorizontalParameter(double p);
        virtual boost::optional<double> verticalParameter() const;
        virtual boost::optional<double> horizontalParameter() const;

        CurveSegmentData toSegmentData() const
        {
            return{
                id(),
                start(), end(),
                previous(), following(),
                key(), toSegmentSpecificData()};
        }

    signals:
        void dataChanged();
        void previousChanged();
        void followingChanged();
        void startChanged();
        void endChanged();

    protected:
        virtual void on_startChanged() = 0;
        virtual void on_endChanged() = 0;

        virtual QVariant toSegmentSpecificData() const = 0;

        mutable std::vector<QPointF> m_data; // A data cache.
        mutable bool m_valid{}; // Used to perform caching.
        // TODO it seems that m_valid is never true.

        Curve::Point m_start, m_end;

    private:
        Id<CurveSegmentModel> m_previous, m_following;
};


using DefaultCurveSegmentModel = PowerCurveSegmentModel;
