#include "GammaCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include <cmath>

CurveSegmentModel*GammaCurveSegmentModel::clone(
        const id_type<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new GammaCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->gamma = gamma;
    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

QString GammaCurveSegmentModel::name() const
{
    return "Gamma";
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
    if(numInterp + 1 != m_data.size())
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

void GammaCurveSegmentModel::setVerticalParameter(double p)
{
    gamma = p;
    emit dataChanged();
}


boost::optional<double> GammaCurveSegmentModel::verticalParameter() const
{
    return gamma;
}
