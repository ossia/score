
#include <iscore/serialization/VisitorCommon.hpp>
#include <QPoint>
#include <cmath>
#include <cstddef>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include "PowerCurveSegmentModel.hpp"

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
namespace Curve
{

PowerSegment::PowerSegment(
        const SegmentData& dat,
        QObject* parent):
    SegmentModel{dat, parent},
    gamma{dat.specificSegmentData.value<PowerSegmentData>().gamma}
{

}

SegmentModel*PowerSegment::clone(
        const Id<SegmentModel>& id,
        QObject* parent) const
{
    auto cs = new PowerSegment{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->gamma = gamma;
    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

UuidKey<Curve::SegmentFactory> PowerSegment::concreteFactoryKey() const
{
    return PowerSegmentData::static_concreteFactoryKey();
}

void PowerSegment::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void PowerSegment::on_startChanged()
{
    emit dataChanged();
}

void PowerSegment::on_endChanged()
{
    emit dataChanged();
}

void PowerSegment::updateData(int numInterp) const
{
    if(std::size_t(numInterp + 1) != m_data.size())
        m_valid = false;
    if(!m_valid)
    {
        if(gamma == 12.05)
        {
            if(m_data.size() != 2)
                m_data.resize(2);
            m_data[0] = start();
            m_data[1] = end();
        }
        else
        {
            numInterp = 75;
            m_data.resize(numInterp + 1);
            double start_x = start().x();
            double start_y = start().y();
            double end_x = end().x();
            double end_y = end().y();

            for(int j = 0; j <= numInterp; j++)
            {
                QPointF& pt = m_data[j];
                pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
                pt.setY(start_y + std::pow(double(j) / numInterp, 12.05 - gamma) * (end_y - start_y));
            }
        }
    }
}

double PowerSegment::valueAt(double x) const
{
    return start().y() + (end().y() - start().y()) * (x - start().x()) / (end().x() - start().x());

    return -1;
}

void PowerSegment::setVerticalParameter(double p)
{
    if(start().y() < end().y())
        gamma = (p + 1) * 6.;
    else
        gamma = (1 - p) * 6.;
    emit dataChanged();
}


boost::optional<double> PowerSegment::verticalParameter() const
{
    return gamma / 6. - 1;
}

const UuidKey<Curve::SegmentFactory>&PowerSegmentData::static_concreteFactoryKey()
{
    static const UuidKey<Curve::SegmentFactory> name{"1e7cb83f-4e47-4b14-814d-2242a9c75991"};
    return name;
}
}
