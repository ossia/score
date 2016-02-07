
#include <iscore/serialization/VisitorCommon.hpp>
#include <QPoint>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include "LinearCurveSegmentModel.hpp"

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
SegmentModel* LinearSegment::clone(
        const Id<SegmentModel>& id,
        QObject *parent) const
{
    auto cs = new LinearSegment{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

UuidKey<Curve::SegmentFactory> LinearSegment::concreteFactoryKey() const
{
    return LinearSegmentData::static_concreteFactoryKey();
}

void LinearSegment::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

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

const UuidKey<Curve::SegmentFactory>& LinearSegmentData::static_concreteFactoryKey()
{
    static const UuidKey<Curve::SegmentFactory> name{"a8bd14e2-d7e4-47cd-b76a-6a88fa11f0d2"};
    return name;
}
}
