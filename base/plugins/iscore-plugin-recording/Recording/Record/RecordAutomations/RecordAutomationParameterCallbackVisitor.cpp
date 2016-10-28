#include "RecordAutomationParameterCallbackVisitor.hpp"
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Automation/AutomationModel.hpp>

namespace Recording
{

void RecordAutomationParameterCallbackVisitor::operator()(std::array<float, 2> val)
{
    const double msec = msecs.msec();
    auto it = recorder.vec2_records.find(addr);
    ISCORE_ASSERT(it != recorder.vec2_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 2;
    for(std::size_t i = 0; i < N; i++)
    {
        const RecordData& proc = proc_data[i];
        auto last = proc.segment.points().rbegin();
        proc.segment.addPoint(msec - 1, last->second);
        proc.segment.addPoint(msec, val[i]);
        static_cast<Automation::ProcessModel*>(proc.curveModel.parent())->setDuration(msecs);
    }
}

void RecordAutomationParameterCallbackVisitor::operator()(std::array<float, 3> val)
{
    const double msec = msecs.msec();
    auto it = recorder.vec3_records.find(addr);
    ISCORE_ASSERT(it != recorder.vec3_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 3;
    for(std::size_t i = 0; i < N; i++)
    {
        const RecordData& proc = proc_data[i];
        auto last = proc.segment.points().rbegin();
        proc.segment.addPoint(msec - 1, last->second);
        proc.segment.addPoint(msec, val[i]);
        static_cast<Automation::ProcessModel*>(proc.curveModel.parent())->setDuration(msecs);
    }

}

void RecordAutomationParameterCallbackVisitor::operator()(std::array<float, 4> val)
{
    const double msec = msecs.msec();
    auto it = recorder.vec4_records.find(addr);
    ISCORE_ASSERT(it != recorder.vec4_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 4;
    for(std::size_t i = 0; i < N; i++)
    {
        const RecordData& proc = proc_data[i];
        auto last = proc.segment.points().rbegin();
        proc.segment.addPoint(msec - 1, last->second);
        proc.segment.addPoint(msec, val[i]);
        static_cast<Automation::ProcessModel*>(proc.curveModel.parent())->setDuration(msecs);
    }
}

void RecordAutomationParameterCallbackVisitor::handle_numeric(float newval)
{
    const double msec = msecs.msec();
    auto it = recorder.numeric_records.find(addr);
    ISCORE_ASSERT(it != recorder.numeric_records.end());

    const RecordData& proc_data = it->second;

    auto last = proc_data.segment.points().rbegin();
    proc_data.segment.addPoint(msec - 1, last->second);
    proc_data.segment.addPoint(msec, newval);
    static_cast<Automation::ProcessModel*>(proc_data.curveModel.parent())->setDuration(msecs);
}

void RecordAutomationParameterCallbackVisitor::operator()(float f) { handle_numeric(f); }

void RecordAutomationParameterCallbackVisitor::operator()(int f) { handle_numeric(f); }

void RecordAutomationParameterCallbackVisitor::operator()(char f) { handle_numeric(f); }

void RecordAutomationParameterCallbackVisitor::operator()(bool f) { handle_numeric(f); }

}
