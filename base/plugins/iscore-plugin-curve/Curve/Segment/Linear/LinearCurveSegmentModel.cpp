#include "LinearCurveSegmentModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

CurveSegmentModel* LinearCurveSegmentModel::clone(
        const Id<CurveSegmentModel>& id,
        QObject *parent) const
{
    auto cs = new LinearCurveSegmentModel{id, parent};
    cs->setStart(this->start());
    cs->setEnd(this->end());

    // Previous and following shall be set afterwards by the cloner.
    return cs;
}

const CurveSegmentFactoryKey& LinearCurveSegmentModel::key() const
{
    return LinearCurveSegmentData::key();
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

void LinearCurveSegmentModel::updateData(int numInterp) const
{
    if(!m_valid)
    {
        if(m_data.size() != 2)
            m_data.resize(2);
        m_data[0] = start();
        m_data[1] = end();
    }
}

double LinearCurveSegmentModel::valueAt(double x) const
{
    return start().y() + (end().y() - start().y()) * (x - start().x()) / (end().x() - start().x());
}

QVariant LinearCurveSegmentModel::toSegmentSpecificData() const
{
    return QVariant::fromValue(data_type{});
}

const CurveSegmentFactoryKey& LinearCurveSegmentData::key()
{
    static const CurveSegmentFactoryKey name{"Linear"};
    return name;
}
