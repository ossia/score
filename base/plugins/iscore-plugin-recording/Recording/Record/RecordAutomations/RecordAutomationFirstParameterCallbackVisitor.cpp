#include "RecordAutomationFirstParameterCallbackVisitor.hpp"
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Automation/AutomationModel.hpp>

namespace Recording
{

void RecordAutomationFirstParameterCallbackVisitor::operator()(std::array<float, 2> val)
{
    auto it = recorder.vec2_records.find(addr);
    ISCORE_ASSERT(it != recorder.vec2_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 2;
    for(std::size_t i = 0; i < N; i++)
        proc_data[i].segment.addPoint(0, val[i]);
}

void RecordAutomationFirstParameterCallbackVisitor::operator()(std::array<float, 3> val)
{
    auto it = recorder.vec3_records.find(addr);
    ISCORE_ASSERT(it != recorder.vec3_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 3;
    for(std::size_t i = 0; i < N; i++)
        proc_data[i].segment.addPoint(0, val[i]);

}

void RecordAutomationFirstParameterCallbackVisitor::operator()(std::array<float, 4> val)
{
    auto it = recorder.vec4_records.find(addr);
    ISCORE_ASSERT(it != recorder.vec4_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 4;
    for(std::size_t i = 0; i < N; i++)
        proc_data[i].segment.addPoint(0, val[i]);
}

void RecordAutomationFirstParameterCallbackVisitor::handle_numeric(float newval)
{
    auto it = recorder.numeric_records.find(addr);
    ISCORE_ASSERT(it != recorder.numeric_records.end());

    const auto& proc_data = it->second;
    proc_data.segment.addPoint(0, newval);
}

void RecordAutomationFirstParameterCallbackVisitor::operator()(float f) { handle_numeric(f); }

void RecordAutomationFirstParameterCallbackVisitor::operator()(int f) { handle_numeric(f); }

void RecordAutomationFirstParameterCallbackVisitor::operator()(char f) { handle_numeric(f); }

void RecordAutomationFirstParameterCallbackVisitor::operator()(bool f) { handle_numeric(f); }

}
