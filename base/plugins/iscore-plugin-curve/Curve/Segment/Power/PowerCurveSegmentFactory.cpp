#include "PowerCurveSegmentFactory.hpp"
#include "PowerCurveSegmentModel.hpp"

QString PowerCurveSegmentFactory::name() const
{
    return "Power"; // todo boost hashed_unique will save us
}

CurveSegmentModel* PowerCurveSegmentFactory::make(
        const Id<CurveSegmentModel>& id,
        QObject* parent)
{
    return new PowerCurveSegmentModel{id, parent};
}

CurveSegmentModel* PowerCurveSegmentFactory::load(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new PowerCurveSegmentModel{deserializer, parent};});
}
