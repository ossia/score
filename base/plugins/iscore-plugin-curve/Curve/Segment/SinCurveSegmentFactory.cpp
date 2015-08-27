#include "SinCurveSegmentFactory.hpp"
#include "SinCurveSegmentModel.hpp"

QString SinCurveSegmentFactory::name() const
{
    return "Sin"; // boost hashed_unique will save us
}

CurveSegmentModel* SinCurveSegmentFactory::make(
        const Id<CurveSegmentModel>& id,
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
