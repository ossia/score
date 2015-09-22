#include "InitAutomation.hpp"
#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"


InitAutomation::InitAutomation(
        Path<AutomationModel>&& path,
        const iscore::Address& newaddr,
        double newmin,
        double newmax,
        const QVector<QByteArray>& segments):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_path{path},
    m_addr(newaddr),
    m_newMin{newmin},
    m_newMax{newmax},
    m_segments{segments}
{
}

void InitAutomation::undo()
{

}

void InitAutomation::redo()
{
    auto& autom = m_path.find();

    autom.setMin(m_newMin);
    autom.setMax(m_newMax);

    autom.setAddress(m_addr);

    auto& curve = autom.curve();

    curve.clear();

    for(const auto& elt : m_segments)
    {
        Deserializer<DataStream> des(elt);
        curve.addSegment(createCurveSegment(des, &curve));
    }

    curve.changed();
}

void InitAutomation::serializeImpl(QDataStream& s) const
{
    s << m_path << m_addr << m_newMin << m_newMax << m_segments;
}

void InitAutomation::deserializeImpl(QDataStream& s)
{
    s << m_path << m_addr << m_newMin << m_newMax << m_segments;
}
