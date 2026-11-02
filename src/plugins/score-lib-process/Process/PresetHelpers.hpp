#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <ossia/detail/algorithms.hpp>

#include <optional>

namespace Process
{

template <typename T = Process::ControlInlet>
void saveFixedControls(JSONReader& r, const Process::ProcessModel& proc)
{
  r.stream.StartArray();
  for(const auto& inlet : proc.inlets())
  {
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(inlet))
    {
      r.stream.StartArray();
      r.stream.Int(ctrl->id().val());
      r.readFrom(ctrl->value());
      r.stream.EndArray();
    }
  }
  r.stream.EndArray();
}

template <typename T = Process::ControlInlet>
void loadFixedControls(
    const rapidjson::Document::ConstArray& ctrls, Process::ProcessModel& proc)
{
  for(const auto& arr : ctrls)
  {
    // [id, value]; anything else in a preset file is skipped
    if(!arr.IsArray() || arr.Size() < 2 || !arr[0].IsInt())
      continue;
    const auto& id = arr[0].GetInt();
    ossia::value val = JsonValue{arr[1]}.to<ossia::value>();

    auto it = ossia::find_if(
        proc.inlets(), [&](const auto& inl) { return inl->id().val() == id; });
    if(it != proc.inlets().end())
    {
      Process::Inlet& inlet = **it;
      if(auto ctrl = qobject_cast<T*>(&inlet))
      {
        ctrl->setValue(val);
      }
    }
  }
}

//! A string in a preset's data; none if it is missing or not a string
inline std::optional<QString> presetString(const rapidjson::Value& obj, const char* key)
{
  if(!obj.IsObject())
    return {};
  const auto it = obj.FindMember(key);
  if(it == obj.MemberEnd() || !it->value.IsString())
    return {};
  return QString::fromUtf8(it->value.GetString(), it->value.GetStringLength());
}

//! The "Controls" of a preset's data, if it has them
inline void loadPresetControls(const rapidjson::Value& obj, Process::ProcessModel& proc)
{
  if(!obj.IsObject())
    return;
  if(const auto it = obj.FindMember("Controls");
     it != obj.MemberEnd() && it->value.IsArray())
    loadFixedControls(it->value.GetArray(), proc);
}

template <typename T>
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

template <typename ScriptProperty, typename T>
void loadScriptProcessPreset(T& process, const Process::Preset& preset)
{
  const Process::ProcessModel& base = process;
  if(!preset.key.effect.isEmpty() && preset.key.effect != base.effect())
    if constexpr(requires { (process.*ScriptProperty::set)(preset.key.effect); })
      (void)(process.*ScriptProperty::set)(preset.key.effect);

  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsArray())
    return;
  Process::loadFixedControls(doc.GetArray(), process);
}
}
