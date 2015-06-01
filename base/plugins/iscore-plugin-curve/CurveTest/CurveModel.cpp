#include "CurveModel.hpp"
#include "CurveSegmentModel.hpp"
void CurveModel::addSegment(CurveSegmentModel* m)
{
    m->setParent(this);
    m_segments.append(m);
    emit segmentAdded(m);
}


void CurveModel::removeSegment(CurveSegmentModel* m)
{
    auto index = m_segments.indexOf(m);
    if(index >= 0)
    {
        m_segments.remove(index);
    }

    emit segmentRemoved(m);
}


void CurveModel::clear()
{
    qDeleteAll(m_segments);
    m_segments.clear();
}


const QVector<CurveSegmentModel*>&CurveModel::segments() const
{
    return m_segments;
}
