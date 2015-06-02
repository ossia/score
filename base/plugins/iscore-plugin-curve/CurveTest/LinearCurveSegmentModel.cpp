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

QVector<QPointF> GammaCurveSegmentModel::data(int numInterp) const
{
    QVector<QPointF> interppts;
    interppts.resize(numInterp + 1);

    for(int j = 0; j <= numInterp; j++)
    {
        QPointF& pt = interppts[j];
        pt.setX(start().x() + (double(j) / numInterp) * (end().x() - start().x()));
        pt.setY(start().y() + std::pow(double(j) / numInterp, gamma) * (end().y() - start().y()));
    }

    return interppts;
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

QVector<QPointF> SinCurveSegmentModel::data(int numInterp) const
{
    QVector<QPointF> interppts;
    numInterp *= 2;
    interppts.resize(numInterp + 1);

    for(int j = 0; j <= numInterp; j++)
    {
        QPointF& pt = interppts[j];
        pt.setX(start().x() + (double(j) / numInterp) * (end().x() - start().x()));
        pt.setY(0.5 + 0.5 * ampl * sin(6.28 * freq * double(j) / numInterp));
    }

    return interppts;
}
