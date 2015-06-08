#include "SinCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>


CurveSegmentModel*SinCurveSegmentModel::clone(
        const id_type<CurveSegmentModel>& id,
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

void SinCurveSegmentModel::updateData(int numInterp)
{
    if(2*numInterp+1 != m_data.size())
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

void SinCurveSegmentModel::setVerticalParameter(double p)
{
    freq = p;
    emit dataChanged();
}

void SinCurveSegmentModel::setHorizontalParameter(double p)
{
    ampl = p;
    emit dataChanged();
}
