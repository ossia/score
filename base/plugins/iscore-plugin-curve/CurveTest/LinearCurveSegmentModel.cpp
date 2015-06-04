#include "LinearCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

CurveSegmentModel*LinearCurveSegmentModel::clone(
        const id_type<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new LinearCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

QString LinearCurveSegmentModel::name() const
{
    return "Linear";
}

void LinearCurveSegmentModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

void LinearCurveSegmentModel::on_startChanged()
{
    emit dataChanged();
}

void LinearCurveSegmentModel::on_endChanged()
{
    emit dataChanged();
}

void LinearCurveSegmentModel::updateData(int numInterp)
{
    if(!m_valid)
    {
        m_data.resize(2);
        m_data[0] = start();
        m_data[1] = end();
    }
}





CurveSegmentModel*GammaCurveSegmentModel::clone(
        const id_type<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new GammaCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

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

void GammaCurveSegmentModel::updateData(int numInterp)
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



CurveSegmentModel*SinCurveSegmentModel::clone(
        const id_type<CurveSegmentModel>& id,
        QObject* parent) const
{
    auto cs = new SinCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

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
