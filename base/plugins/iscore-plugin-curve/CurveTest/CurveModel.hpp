#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selection.hpp>
class CurveSegmentModel;
class CurvePointModel;

class CurveModel : public IdentifiedObject<CurveModel>
{

        Q_OBJECT
    public:
        CurveModel(const id_type<CurveModel>&, QObject* parent);
        void addSegment(CurveSegmentModel* m);
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
        void segmentAdded(CurveSegmentModel*);
        void segmentRemoved(CurveSegmentModel*); // dangerous if async
        void pointAdded(CurvePointModel*);
        void pointRemoved(CurvePointModel*); // dangerous if async

    private:
        void addPoint(CurvePointModel* pt);
        void removePoint(CurvePointModel* pt);

        IdContainer<ModelMap, CurveSegmentModel> m_segments;
        //QVector<CurveSegmentModel*> m_segments; // Each between 0, 1
        QVector<CurvePointModel*> m_points; // Each between 0, 1
};
