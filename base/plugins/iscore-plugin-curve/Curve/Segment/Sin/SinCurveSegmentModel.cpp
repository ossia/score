#include "SinCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include <cmath>

SinCurveSegmentModel::SinCurveSegmentModel(
        const CurveSegmentData& dat,
        QObject* parent):
    CurveSegmentModel{dat, parent}
{
    const auto& sin_data = dat.specificSegmentData.value<SinCurveSegmentData>();
    freq = sin_data.freq;
    ampl = sin_data.ampl;
}


CurveSegmentModel*SinCurveSegmentModel::clone(
        const Id<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new SinCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    cs->freq = freq;
    cs->ampl = ampl;

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

QString SinCurveSegmentModel::name() const
{
    return "Sin";
}

void SinCurveSegmentModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void SinCurveSegmentModel::on_startChanged()
{
    emit dataChanged();
}

void SinCurveSegmentModel::on_endChanged()
{
    emit dataChanged();
}

void SinCurveSegmentModel::updateData(int numInterp) const
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

double SinCurveSegmentModel::valueAt(double x) const
{
    ISCORE_TODO;
    return -1;
}

void SinCurveSegmentModel::setVerticalParameter(double p)
{
    // From -1; 1 to 0;1
    ampl = (p + 1) / 2.;
    emit dataChanged();
}

void SinCurveSegmentModel::setHorizontalParameter(double p)
{
    // From -1; 1 to 1; 15
    freq = (p + 1) * 7 + 1;
    emit dataChanged();
}

boost::optional<double> SinCurveSegmentModel::verticalParameter() const
{
    return 2. * ampl - 1.;
}

boost::optional<double> SinCurveSegmentModel::horizontalParameter() const
{
    return (freq - 1.) / 7. - 1.;
}
