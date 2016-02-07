#include <iscore/serialization/VisitorCommon.hpp>
#include <QDebug>
#include <QPoint>
#include <cmath>
#include <cstddef>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include "SinCurveSegmentModel.hpp"

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
SinSegment::SinSegment(
        const SegmentData& dat,
        QObject* parent):
    SegmentModel{dat, parent}
{
    const auto& sin_data = dat.specificSegmentData.value<SinSegmentData>();
    freq = sin_data.freq;
    ampl = sin_data.ampl;
}


SegmentModel*SinSegment::clone(
        const Id<SegmentModel>& id,
        QObject* parent) const
{
    auto cs = new SinSegment{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->freq = freq;
    cs->ampl = ampl;

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

UuidKey<Curve::SegmentFactory> SinSegment::concreteFactoryKey() const
{
    return data_type::static_concreteFactoryKey();
}


void SinSegment::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void SinSegment::on_startChanged()
{
    emit dataChanged();
}

void SinSegment::on_endChanged()
{
    emit dataChanged();
}

void SinSegment::updateData(int numInterp) const
{
    if(std::size_t(2 * numInterp + 1) != m_data.size())
        m_valid = false;
    if(!m_valid)
    {
        numInterp *= 2;
        m_data.resize(numInterp + 1);

        double start_x = start().x();
        double end_x = end().x();
        for(int j = 0; j <= numInterp; j++)
        {
            QPointF& pt = m_data[j];
            pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
            pt.setY(0.5 + 0.5 * ampl * sin(6.28 * freq * double(j) / numInterp));
        }
    }
}

double SinSegment::valueAt(double x) const
{
    ISCORE_TODO;
    return -1;
}

void SinSegment::setVerticalParameter(double p)
{
    // From -1; 1 to 0;1
    ampl = (p + 1) / 2.;
    emit dataChanged();
}

void SinSegment::setHorizontalParameter(double p)
{
    // From -1; 1 to 1; 15
    freq = (p + 1) * 7 + 1;
    emit dataChanged();
}

boost::optional<double> SinSegment::verticalParameter() const
{
    return 2. * ampl - 1.;
}

boost::optional<double> SinSegment::horizontalParameter() const
{
    return (freq - 1.) / 7. - 1.;
}


const UuidKey<Curve::SegmentFactory>& SinSegmentData::static_concreteFactoryKey()
{
    static const UuidKey<Curve::SegmentFactory> name{"c16dad6a-a422-4fb7-8bd5-850cbe8c3f28"};
    return name;
}
}
