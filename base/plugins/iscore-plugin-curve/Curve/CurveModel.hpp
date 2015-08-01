#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "Segment/CurveSegmentModel.hpp"
class CurvePointModel;

class CurveModel : public IdentifiedObject<CurveModel>
{
        ISCORE_SERIALIZE_FRIENDS(CurveModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(CurveModel, JSONObject)
        Q_OBJECT
    public:
        CurveModel(const id_type<CurveModel>&, QObject* parent);

        template<typename Impl>
        CurveModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveModel* clone(const id_type<CurveModel>&, QObject* parent);
        void addSegment(CurveSegmentModel* m);

        // Here we don't pass an id because it's more efficient
        void removeSegment(CurveSegmentModel* m);


        Selection selectedChildren() const;
        void setSelection(const Selection& s);

        void clear();

        const auto& segments() const
        {
            return m_segments;
        }
        const QVector<CurvePointModel*>& points() const;

    signals:
        void segmentAdded(const CurveSegmentModel&);
        void segmentRemoved(const id_type<CurveSegmentModel>&); // dangerous if async
        void pointAdded(const CurvePointModel&);
        void pointRemoved(const id_type<CurvePointModel>&); // dangerous if async

        void changed();
        void cleared();

    private:
        void addPoint(CurvePointModel* pt);
        void removePoint(CurvePointModel* pt);

        IdContainer<CurveSegmentModel> m_segments;
        QVector<CurvePointModel*> m_points; // Each between 0, 1
};
