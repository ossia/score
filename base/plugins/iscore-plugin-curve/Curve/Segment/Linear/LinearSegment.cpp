#include <iscore/serialization/VisitorCommon.hpp>
#include <QPoint>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include "LinearSegment.hpp"

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
void LinearSegment::on_startChanged()
{
    emit dataChanged();
}

void LinearSegment::on_endChanged()
{
    emit dataChanged();
}

void LinearSegment::updateData(int numInterp) const
{
    if(!m_valid)
    {
        if(m_data.size() != 2)
            m_data.resize(2);
        m_data[0] = start();
        m_data[1] = end();
    }
}

double LinearSegment::valueAt(double x) const
{
    return start().y() + (end().y() - start().y()) * (x - start().x()) / (end().x() - start().x());
}

QVariant LinearSegment::toSegmentSpecificData() const
{
    return QVariant::fromValue(data_type{});
}
}
