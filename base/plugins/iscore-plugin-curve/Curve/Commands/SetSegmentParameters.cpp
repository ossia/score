#include <boost/core/explicit_operator_bool.hpp>
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

SetSegmentParameters::SetSegmentParameters(
        Path<CurveModel>&& model,
        SegmentParameterMap&& parameters):
    m_model{std::move(model)},
    m_new{std::move(parameters)}
{
    const auto& curve = m_model.find();
    for(const auto& elt : m_new.keys())
    {
        const auto& seg = curve.segments().at(elt);
        m_old.insert(elt, {seg.verticalParameter(), seg.horizontalParameter()});
    }
}

void SetSegmentParameters::undo() const
{
    auto& curve = m_model.find();
    for(const auto& elt : m_old.keys())
    {
        auto& seg = curve.segments().at(elt);

        if(m_old.value(elt).first)
            seg.setVerticalParameter(*m_old.value(elt).first);
        if(m_old.value(elt).second)
            seg.setHorizontalParameter(*m_old.value(elt).second);
    }

    curve.changed();
}

void SetSegmentParameters::redo() const
{
    auto& curve = m_model.find();
    for(const auto& elt : m_new.keys())
    {
        auto& seg = curve.segments().at(elt);

        seg.setVerticalParameter(m_new.value(elt).first);
        seg.setHorizontalParameter(m_new.value(elt).second);
    }

    curve.changed();
}

void SetSegmentParameters::update(Path<CurveModel>&& model, SegmentParameterMap &&segments)
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
