#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <vector>

#include "Segment/CurveSegmentModel.hpp"
#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_curve_export.h>
namespace Curve
{
class PointModel;
struct SegmentData;
class ISCORE_PLUGIN_CURVE_EXPORT Model final : public IdentifiedObject<Model>
{
        ISCORE_SERIALIZE_FRIENDS(Curve::Model, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Curve::Model, JSONObject)
        Q_OBJECT
    public:
        Model(const Id<Model>&, QObject* parent);

        template<typename Impl>
        Model(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        Model* clone(const Id<Model>&, QObject* parent);

        // These two will create points
        void addSegment(SegmentModel* m);
        void addSortedSegment(SegmentModel* m);

        // Won't create points, plain insertion.
        void insertSegment(SegmentModel*);

        // Here we don't pass an id because it's more efficient
        void removeSegment(SegmentModel* m);

        std::vector<SegmentData> toCurveData() const;
        void fromCurveData(const std::vector<SegmentData>& curve);


        Selection selectedChildren() const;
        void setSelection(const Selection& s);

        void clear();

        const auto& segments() const { return m_segments;}
        auto& segments() { return m_segments;}

        const std::vector<PointModel*>& points() const;
        std::vector<PointModel*>& points() {return m_points;}

    signals:
        void segmentAdded(const SegmentModel&);
        void segmentRemoved(const Id<SegmentModel>&); // dangerous if async
        void pointAdded(const PointModel&);
        void pointRemoved(const Id<PointModel>&); // dangerous if async

        // This signal has to be emitted after big modifications.
        // (it's an optimization to prevent updating the OSSIA API each time a segment moves).
        void changed();
        void curveReset(); // like changed() but for the presenter
        void cleared();

    private:
        void addPoint(PointModel* pt);
        void removePoint(PointModel* pt);

        IdContainer<SegmentModel> m_segments;
        std::vector<PointModel*> m_points; // Each between 0, 1
};

std::vector<SegmentData> ISCORE_PLUGIN_CURVE_EXPORT orderedSegments(const Model& curve);
}
