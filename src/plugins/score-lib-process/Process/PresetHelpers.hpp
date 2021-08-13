#pragma once
#include <Process/Process.hpp>
#include <ossia/detail/algorithms.hpp>

#include <Process/Dataflow/Port.hpp>

namespace Process
{

inline void saveFixedControls(JSONReader& r, const Process::ProcessModel& proc)
{
  r.stream.StartArray();
  for (const auto& inlet : proc.inlets())
  {
    if (auto ctrl = qobject_cast<Process::ControlInlet*>(inlet))
    {
      r.stream.StartArray();
      r.stream.Int(ctrl->id().val());
      r.readFrom(ctrl->value());
      r.stream.EndArray();
    }
  }
  r.stream.EndArray();
}

inline void loadFixedControls(const rapidjson::Document::ConstArray& ctrls, Process::ProcessModel& proc)
{
  for (const auto& arr : ctrls)
  {
    const auto& id = arr[0].GetInt();
    ossia::value val = JsonValue{arr[1]}.to<ossia::value>();

    auto it = ossia::find_if(
        proc.inlets(), [&](const auto& inl) { return inl->id().val() == id; });
    if (it != proc.inlets().end())
    {
      Process::Inlet& inlet = **it;
      if (auto ctrl = qobject_cast<Process::ControlInlet*>(&inlet))
      {
        ctrl->setValue(val);
      }
    }
  }
}

}
