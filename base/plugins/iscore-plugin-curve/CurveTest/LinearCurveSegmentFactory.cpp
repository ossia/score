#include "LinearCurveSegmentFactory.hpp"
#include "LinearCurveSegmentModel.hpp"

QString LinearCurveSegmentFactory::name() const
{
    return "Linear"; // boost hashed_unique will save us
}

CurveSegmentModel* LinearCurveSegmentFactory::make(
        const id_type<CurveSegmentModel>& id,
        QObject* parent)
{
    return new LinearCurveSegmentModel{id, parent};
}

CurveSegmentModel* LinearCurveSegmentFactory::load(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new LinearCurveSegmentModel{deserializer, parent};});
}


QString GammaCurveSegmentFactory::name() const
{
    return "Gamma"; // boost hashed_unique will save us
}

CurveSegmentModel* GammaCurveSegmentFactory::make(
        const id_type<CurveSegmentModel>& id,
        QObject* parent)
{
    return new GammaCurveSegmentModel{id, parent};
}

CurveSegmentModel* GammaCurveSegmentFactory::load(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new GammaCurveSegmentModel{deserializer, parent};});
}


QString SinCurveSegmentFactory::name() const
{
    return "Sin"; // boost hashed_unique will save us
}

CurveSegmentModel* SinCurveSegmentFactory::make(
        const id_type<CurveSegmentModel>& id,
        QObject* parent)
{
    return new SinCurveSegmentModel{id, parent};
}

CurveSegmentModel* SinCurveSegmentFactory::load(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new SinCurveSegmentModel{deserializer, parent};});
}
