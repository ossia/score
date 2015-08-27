#include "PowerCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include <cmath>

CurveSegmentModel*PowerCurveSegmentModel::clone(
        const Id<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new PowerCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->gamma = gamma;
    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

QString PowerCurveSegmentModel::name() const
{
    return "Power";
}

void PowerCurveSegmentModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void PowerCurveSegmentModel::on_startChanged()
{
    emit dataChanged();
}

void PowerCurveSegmentModel::on_endChanged()
{
    emit dataChanged();
}

void PowerCurveSegmentModel::updateData(int numInterp) const
{
    if(numInterp + 1 != m_data.size())
        m_valid = false;
    if(!m_valid)
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

double PowerCurveSegmentModel::valueAt(double x) const
{
    ISCORE_TODO;
    return -1;
}

void PowerCurveSegmentModel::setVerticalParameter(double p)
{
    gamma = (p + 1) * 6.;
    emit dataChanged();
}


boost::optional<double> PowerCurveSegmentModel::verticalParameter() const
{
    return gamma / 6. - 1;
}
