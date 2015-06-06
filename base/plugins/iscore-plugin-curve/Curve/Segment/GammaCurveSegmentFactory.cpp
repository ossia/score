#include "GammaCurveSegmentFactory.hpp"
#include "GammaCurveSegmentModel.hpp"

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
