#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <vector>

#include "Segment/CurveSegmentModel.hpp"
#include <iscore/serialization/VisitorInterface.hpp>

class CurvePointModel;
class DataStream;
class JSONObject;
class QObject;
struct CurveSegmentData;
template <typename tag, typename impl> class id_base_t;

class CurveModel final : public IdentifiedObject<CurveModel>
{
        ISCORE_SERIALIZE_FRIENDS(CurveModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(CurveModel, JSONObject)
        Q_OBJECT
    public:
        CurveModel(const Id<CurveModel>&, QObject* parent);

        template<typename Impl>
        CurveModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveModel* clone(const Id<CurveModel>&, QObject* parent);

        // These two will create points
        void addSegment(CurveSegmentModel* m);
        void addSortedSegment(CurveSegmentModel* m);

        // Won't create points, plain insertion.
        void insertSegment(CurveSegmentModel*);

        // Here we don't pass an id because it's more efficient
        void removeSegment(CurveSegmentModel* m);

        std::vector<CurveSegmentData> toCurveData() const;
        void fromCurveData(const std::vector<CurveSegmentData>& curve);


        Selection selectedChildren() const;
        void setSelection(const Selection& s);

        void clear();

        const auto& segments() const { return m_segments;}
        auto& segments() { return m_segments;}

        const std::vector<CurvePointModel*>& points() const;
        std::vector<CurvePointModel*>& points() {return m_points;}

    signals:
        void segmentAdded(const CurveSegmentModel&);
        void segmentRemoved(const Id<CurveSegmentModel>&); // dangerous if async
        void pointAdded(const CurvePointModel&);
        void pointRemoved(const Id<CurvePointModel>&); // dangerous if async

        // This signal has to be emitted after big modifications.
        // (it's an optimization to prevent updating the OSSIA API each time a segment moves).
        void changed();
        void curveReset(); // like changed() but for the presenter
        void cleared();

    private:
        void addPoint(CurvePointModel* pt);
        void removePoint(CurvePointModel* pt);

        IdContainer<CurveSegmentModel> m_segments;
        std::vector<CurvePointModel*> m_points; // Each between 0, 1
};

namespace Curve {
std::vector<CurveSegmentData> orderedSegments(const CurveModel& curve);
}
