#include "InitAutomation.hpp"
#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"


InitAutomation::InitAutomation(
        Path<AutomationModel>&& path,
        const iscore::Address& newaddr,
        double newmin,
        double newmax,
        std::vector<CurveSegmentData>&& segments):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_path{path},
    m_addr(newaddr),
    m_newMin{newmin},
    m_newMax{newmax},
    m_segments{std::move(segments)}
{
}

void InitAutomation::undo() const
{

}

void InitAutomation::redo() const
{
    auto& autom = m_path.find();

    auto& curve = autom.curve();
    curve.fromCurveData(m_segments);

    autom.setMin(m_newMin);
    autom.setMax(m_newMax);

    autom.setAddress(m_addr);
}

void InitAutomation::serializeImpl(QDataStream& s) const
{
    s << m_path << m_addr << m_newMin << m_newMax << m_segments;
}

void InitAutomation::deserializeImpl(QDataStream& s)
{
    s << m_path << m_addr << m_newMin << m_newMax << m_segments;
}
