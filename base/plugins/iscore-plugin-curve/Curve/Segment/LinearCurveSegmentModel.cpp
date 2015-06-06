#include "LinearCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

CurveSegmentModel*LinearCurveSegmentModel::clone(
        const id_type<CurveSegmentModel>& id,
        QObject *parent) const
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





