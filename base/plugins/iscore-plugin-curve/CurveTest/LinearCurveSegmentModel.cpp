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

QVector<QPointF> LinearCurveSegmentModel::data(int numInterp) const
{
    QVector<QPointF> interppts;
    interppts.resize(numInterp + 1);

    for(int j = 0; j <= numInterp; j++)
    {
        interppts[j] = start() + double(j) / numInterp * (end() - start());
    }

    return interppts;
}
