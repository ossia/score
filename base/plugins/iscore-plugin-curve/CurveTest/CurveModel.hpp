#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
class CurveSegmentModel;

class CurveModel : public QObject
{
        QVector<CurveSegmentModel*> m_segments; // Each between 0, 1

        Q_OBJECT
    public:
        void addSegment(CurveSegmentModel* m);
        void removeSegment(CurveSegmentModel* m);

        void clear();

        const QVector<CurveSegmentModel*>& segments() const;

    signals:
        void segmentAdded(CurveSegmentModel*);
        void segmentRemoved(CurveSegmentModel*);
};
