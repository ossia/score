#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
class CurveSegmentModel;

class CurveModel : public IdentifiedObject<CurveModel>
{
        QVector<CurveSegmentModel*> m_segments; // Each between 0, 1

        Q_OBJECT
    public:
        CurveModel(const id_type<CurveModel>&, QObject* parent);
        void addSegment(CurveSegmentModel* m);
        void removeSegment(CurveSegmentModel* m);

        void clear();

        const QVector<CurveSegmentModel*>& segments() const;

    signals:
        void segmentAdded(CurveSegmentModel*);
        void segmentRemoved(CurveSegmentModel*);
};
