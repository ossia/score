#pragma once
#include <Automation/AutomationModel.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Recording/Record/RecordManager.hpp>

namespace Recording
{
/**
 * @brief The ParameterPolicy struct
 *
 * This policy is used to prevent interpolation :
 * if a message "/x 1" is received at t = 0,
 * if a message "/x 2" is received at t = 1,
 * then at t = 0.5 "/x" will still be 1
 */
struct ParameterPolicy
{
  void operator()(const RecordData& proc, const TimeVal& msecs, double msec, float val)
  {
    auto last = proc.segment.points().rbegin();
    proc.segment.addPoint(msec - 1, last->second);
    proc.segment.addPoint(msec, val);
    static_cast<Automation::ProcessModel*>(proc.curveModel.parent())->setDuration(msecs);
  }
};

/**
 * @brief The MessagePolicy struct
 *
 * This policy is used to interpolate :
 * if a message "/x 1" is received at t = 0,
 * if a message "/x 2" is received at t = 1,
 * then at t = 0.5 "/x" will be 1.5
 */
struct MessagePolicy
{
  void operator()(const RecordData& proc, const TimeVal& msecs, double msec, float val)
  {
    proc.segment.addPoint(msec, val);
    static_cast<Automation::ProcessModel*>(proc.curveModel.parent())->setDuration(msecs);
  }
};

template <typename RecordingPolicy>
struct RecordAutomationSubsequentCallbackVisitor
{
  AutomationRecorder& recorder;
  const State::Address& addr;
  TimeVal msecs;

  void operator()(std::array<float, 2> val)
  {
    const double msec = msecs.msec();
    auto it = recorder.vec2_records.find(addr);
    SCORE_ASSERT(it != recorder.vec2_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 2;
    for (std::size_t i = 0; i < N; i++)
    {
      RecordingPolicy{}(proc_data[i], msecs, msec, val[i]);
    }
  }

  void operator()(std::array<float, 3> val)
  {
    const double msec = msecs.msec();
    auto it = recorder.vec3_records.find(addr);
    SCORE_ASSERT(it != recorder.vec3_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 3;
    for (std::size_t i = 0; i < N; i++)
    {
      RecordingPolicy{}(proc_data[i], msecs, msec, val[i]);
    }
  }

  void operator()(std::array<float, 4> val)
  {
    const double msec = msecs.msec();
    auto it = recorder.vec4_records.find(addr);
    SCORE_ASSERT(it != recorder.vec4_records.end());

    const auto& proc_data = it->second;

    const constexpr std::size_t N = 4;
    for (std::size_t i = 0; i < N; i++)
    {
      RecordingPolicy{}(proc_data[i], msecs, msec, val[i]);
    }
  }

  void handle_numeric(float newval)
  {
    const double msec = msecs.msec();
    auto it = recorder.numeric_records.find(addr);
    SCORE_ASSERT(it != recorder.numeric_records.end());

    const RecordData& proc_data = it->second;

    RecordingPolicy{}(proc_data, msecs, msec, newval);
  }

  void operator()(float f) { handle_numeric(f); }

  void operator()(int f) { handle_numeric(f); }

  void operator()(char f) { handle_numeric(f); }

  void operator()(bool f) { handle_numeric(f); }

  template <typename... T>
  void operator()(const T&...)
  {
  }
};
}
