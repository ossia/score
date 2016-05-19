
#include <QDataStream>
#include <QtGlobal>
#include <algorithm>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include "SetSegmentParameters.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Curve
{
SetSegmentParameters::SetSegmentParameters(
        Path<Model>&& model,
        SegmentParameterMap&& parameters):
    m_model{std::move(model)},
    m_new{std::move(parameters)}
{
    const auto& curve = m_model.find();
    for(auto it = m_new.cbegin(); it != m_new.cend(); ++it)
    {
        const auto& seg = curve.segments().at(it.key());
        m_old.insert(it.key(), {seg.verticalParameter(), seg.horizontalParameter()});
    }
}

void SetSegmentParameters::undo() const
{
    auto& curve = m_model.find();
    for(auto it = m_old.cbegin(); it != m_old.cend(); ++it)
    {
        auto& seg = curve.segments().at(it.key());

        if(it.value().first)
            seg.setVerticalParameter(*it.value().first);
        if(it.value().second)
            seg.setHorizontalParameter(*it.value().second);
    }

    curve.changed();
}

void SetSegmentParameters::redo() const
{
    auto& curve = m_model.find();
    for(auto it = m_new.cbegin(); it != m_new.cend(); ++it)
    {
        auto& seg = curve.segments().at(it.key());

        seg.setVerticalParameter(it.value().first);
        seg.setHorizontalParameter(it.value().second);
    }

    curve.changed();
}

void SetSegmentParameters::update(Path<Model>&& model, SegmentParameterMap &&segments)
{
    m_new = std::move(segments);
}

void SetSegmentParameters::serializeImpl(DataStreamInput& s) const
{
   s << m_model << m_old << m_new;
}

void SetSegmentParameters::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_old >> m_new;
}
}
