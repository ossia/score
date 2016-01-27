#include <iscore/serialization/VisitorCommon.hpp>
#include <QDebug>
#include <QPoint>
#include <cmath>
#include <cstddef>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include "GammaCurveSegmentModel.hpp"

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
namespace Curve
{
GammaSegment::GammaSegment(
        const SegmentData& dat,
        QObject* parent):
    SegmentModel{dat, parent},
    gamma{dat.specificSegmentData.value<GammaSegmentData>().gamma}
{

}

SegmentModel*GammaSegment::clone(
        const Id<SegmentModel>& id,
        QObject* parent) const
{
    auto cs = new GammaSegment{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->gamma = gamma;
    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

SegmentFactoryKey GammaSegment::concreteFactoryKey() const
{
    static const SegmentFactoryKey name{"a8bd14e2-d7e4-47cd-b76a-6a88fa11f0d2"};
    return name;
}

void GammaSegment::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void GammaSegment::on_startChanged()
{
    emit dataChanged();
}

void GammaSegment::on_endChanged()
{
    emit dataChanged();
}

void GammaSegment::updateData(int numInterp) const
{
    if(std::size_t(numInterp + 1) != m_data.size())
        m_valid = false;
    if(!m_valid)
    {
        m_data.resize(numInterp + 1);
        double start_x = start().x();
        double start_y = start().y();
        double end_x = end().x();
        double end_y = end().y();
        for(int j = 0; j <= numInterp; j++)
        {
            QPointF& pt = m_data[j];
            pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
            pt.setY(start_y + std::pow(double(j) / numInterp, gamma) * (end_y - start_y));
        }
    }
}

double GammaSegment::valueAt(double x) const
{
    ISCORE_TODO;
    return -1;
}

void GammaSegment::setVerticalParameter(double p)
{
    gamma = p;
    emit dataChanged();
}


boost::optional<double> GammaSegment::verticalParameter() const
{
    return gamma;
}
}
