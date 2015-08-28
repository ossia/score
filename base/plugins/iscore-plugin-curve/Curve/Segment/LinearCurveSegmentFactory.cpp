#include "LinearCurveSegmentFactory.hpp"
#include "LinearCurveSegmentModel.hpp"

QString LinearCurveSegmentFactory::name() const
{
    return "Linear"; // boost hashed_unique will save us
}

CurveSegmentModel* LinearCurveSegmentFactory::make(
        const Id<CurveSegmentModel>& id,
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


