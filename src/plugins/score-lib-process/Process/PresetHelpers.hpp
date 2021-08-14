#pragma once
#include <Process/Process.hpp>
#include <ossia/detail/algorithms.hpp>

#include <Process/Dataflow/Port.hpp>

namespace Process
{

template<typename T = Process::ControlInlet>
void saveFixedControls(JSONReader& r, const Process::ProcessModel& proc)
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

template<typename T = Process::ControlInlet>
void loadFixedControls(const rapidjson::Document::ConstArray& ctrls, Process::ProcessModel& proc)
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
      if (auto ctrl = qobject_cast<T*>(&inlet))
      {
        ctrl->setValue(val);
      }
    }
  }
}

template<typename T>
Process::Preset saveScriptProcessPreset(const T& process, const QString& data)
{
  Process::Preset p;
  p.name = process.metadata().getName();
  p.key.key = Metadata<ConcreteKey_k, T>::get();
  p.key.effect = data;

  JSONReader r;
  Process::saveFixedControls(r, process);

  p.data = r.toByteArray();
  return p;
}

template<typename ScriptProperty, typename T>
void loadScriptProcessPreset(T& process, const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsArray())
    return;
  Process::loadFixedControls(doc.GetArray(), process);
}
}
