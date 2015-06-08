#include "SetSegmentParameters.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"

SetSegmentParameters::SetSegmentParameters(
        ObjectPath&& model,
        SegmentParameterMap&& parameters):
    iscore::SerializableCommand("CurveControl", className(), description()),
    m_model{std::move(model)},
    m_new{std::move(parameters)}
{
    const auto& curve = m_model.find<CurveModel>();
    for(const auto& elt : m_new.keys())
    {
        CurveSegmentModel* seg = curve.segments().at(elt);
        m_old.insert(elt, {seg->verticalParameter(), seg->horizontalParameter()});
    }
}

void SetSegmentParameters::undo()
{
    auto& curve = m_model.find<CurveModel>();
    /*
    curve.clear();

    for(const auto& elt : m_oldCurveData)
    {
        Deserializer<DataStream> des(elt);
        curve.addSegment(createCurveSegment(des, &curve));
    }*/
}

void SetSegmentParameters::redo()
{
    auto& curve = m_model.find<CurveModel>();
    /*
    curve.clear();

    for(const auto& elt : m_newCurveData)
    {
        Deserializer<DataStream> des(elt);
        curve.addSegment(createCurveSegment(des, &curve));
    }*/
}

void SetSegmentParameters::update(ObjectPath&& model, SegmentParameterMap &&segments)
{
   // m_newCurveData = std::move(segments);
}

void SetSegmentParameters::serializeImpl(QDataStream& s) const
{
   // s << m_model << m_oldCurveData << m_newCurveData;
}

void SetSegmentParameters::deserializeImpl(QDataStream& s)
{
   // s >> m_model >> m_oldCurveData >> m_newCurveData;
}
