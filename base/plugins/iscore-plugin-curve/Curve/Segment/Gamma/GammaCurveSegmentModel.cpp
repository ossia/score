#include <iscore/serialization/VisitorCommon.hpp>
#include <qdebug.h>
#include <qpoint.h>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Curve/Palette/CurvePoint.hpp"
#include "Curve/Segment/CurveSegmentData.hpp"
#include "GammaCurveSegmentModel.hpp"

class QObject;
template <typename tag, typename impl> class id_base_t;

GammaCurveSegmentModel::GammaCurveSegmentModel(
        const CurveSegmentData& dat,
        QObject* parent):
    CurveSegmentModel{dat, parent},
    gamma{dat.specificSegmentData.value<GammaCurveSegmentData>().gamma}
{

}

CurveSegmentModel*GammaCurveSegmentModel::clone(
        const Id<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new GammaCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->gamma = gamma;
    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

const CurveSegmentFactoryKey& GammaCurveSegmentModel::key() const
{
    static const CurveSegmentFactoryKey name{"Gamma"};
    return name;
}

void GammaCurveSegmentModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void GammaCurveSegmentModel::on_startChanged()
{
    emit dataChanged();
}

void GammaCurveSegmentModel::on_endChanged()
{
    emit dataChanged();
}

void GammaCurveSegmentModel::updateData(int numInterp) const
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

double GammaCurveSegmentModel::valueAt(double x) const
{
    ISCORE_TODO;
    return -1;
}

void GammaCurveSegmentModel::setVerticalParameter(double p)
{
    gamma = p;
    emit dataChanged();
}


boost::optional<double> GammaCurveSegmentModel::verticalParameter() const
{
    return gamma;
}
