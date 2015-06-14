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
        friend void Visitor<Writer<DataStream>>::writeTo<CurveModel>(CurveModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<CurveModel>(CurveModel& ev);
        Q_OBJECT
    public:
        CurveModel(const id_type<CurveModel>&, QObject* parent);

        template<typename Impl>
        CurveModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<CurveModel>{vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveModel* clone(const id_type<CurveModel>&, QObject* parent);
        void addSegment(CurveSegmentModel* m);
        void removeSegment(CurveSegmentModel* m); // TODO why not id?

        Selection selectedChildren() const;
        void setSelection(const Selection& s);

        void clear();

        const auto& segments() const
        {
            return m_segments;
        }
        const QVector<CurvePointModel*>& points() const;

    signals:
        void segmentAdded(CurveSegmentModel*);
        void segmentRemoved(CurveSegmentModel*); // dangerous if async
        void pointAdded(CurvePointModel*);
        void pointRemoved(CurvePointModel*); // dangerous if async

        void cleared();

    private:
        void addPoint(CurvePointModel* pt);
        void removePoint(CurvePointModel* pt);

        IdContainer<CurveSegmentModel> m_segments;
        //QVector<CurveSegmentModel*> m_segments; // Each between 0, 1
        QVector<CurvePointModel*> m_points; // Each between 0, 1
};
