#include "ProcessState.hpp"

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>

namespace Process
{
// A preset's data is an array of controls, or an object with a "Controls"
// member next to other data; anything else is kept whole.
namespace
{
bool hasMembersBesidesControls(const rapidjson::Value& obj)
{
  for(auto& m : obj.GetObject())
    if(m.name != "Controls")
      return true;
  return false;
}

void writeMembersBesidesControls(JsonWriter& w, const rapidjson::Value& obj)
{
  w.StartObject();
  for(auto& m : obj.GetObject())
    if(m.name != "Controls")
    {
      w.Key(m.name.GetString(), m.name.GetStringLength());
      m.value.Accept(w);
    }
  w.EndObject();
}
}

bool recallsState(const ProcessModel& proc) noexcept
{
  return bool(proc.flags() & ProcessFlags::TimeIndependent);
}

QByteArray stateBeyondControls(const ProcessModel& proc)
{
  if(!recallsState(proc))
    return {};
  const auto preset = proc.savePreset();
  const rapidjson::Document data = readJson(preset.data);

  JSONReader r;
  r.stream.StartObject();
  bool any = false;
  if(!preset.key.effect.isEmpty())
  {
    r.obj["Script"] = preset.key.effect;
    any = true;
  }
  if(data.IsObject())
  {
    if(hasMembersBesidesControls(data))
    {
      r.stream.Key("Data");
      writeMembersBesidesControls(r.stream, data);
      any = true;
    }
  }
  else if(!data.IsArray() && !data.IsNull())
  {
    r.stream.Key("Data");
    data.Accept(r.stream);
    any = true;
  }
  else if(data.IsNull() && !preset.data.trimmed().isEmpty()
          && preset.data.trimmed() != "null")
  {
    // Preset data that is not JSON, e.g. an LV2 state in Turtle
    r.stream.Key("Text");
    r.stream.String(preset.data.constData(), preset.data.size());
    any = true;
  }

  // A state's control messages may be sent before the ports they target exist
  if(any)
  {
    const rapidjson::Value* controls{};
    if(data.IsArray())
      controls = &data;
    else if(data.IsObject())
      if(auto it = data.FindMember("Controls"); it != data.MemberEnd())
        controls = &it->value;
    if(controls)
    {
      r.stream.Key("Controls");
      controls->Accept(r.stream);
    }
  }
  if(any)
    r.obj["Process"]
        = QString::fromLatin1(score::uuids::toByteArray(proc.concreteKey().impl()));
  r.stream.EndObject();
  return any ? r.toByteArray() : QByteArray{};
}

namespace
{
// Compares the program and its data, ignoring control values
bool sameProgram(const rapidjson::Document& a, const rapidjson::Document& b)
{
  for(auto key : {"Script", "Data", "Text"})
  {
    auto x = a.FindMember(key);
    auto y = b.FindMember(key);
    const bool hx = x != a.MemberEnd();
    const bool hy = y != b.MemberEnd();
    if(hx != hy || (hx && x->value != y->value))
      return false;
  }
  return true;
}
}

void applyStateBeyondControls(ProcessModel& proc, const QByteArray& state)
{
  const rapidjson::Document st = readJson(state);
  if(!st.IsObject())
    return;
  // Nothing beyond the controls to apply
  auto has = [&](const char* key, bool (rapidjson::Value::*is)() const) {
    auto it = st.FindMember(key);
    return it != st.MemberEnd() && (it->value.*is)();
  };
  if(!has("Script", &rapidjson::Value::IsString) && !st.HasMember("Data")
     && !has("Text", &rapidjson::Value::IsString))
    return;
  // The state of another kind of process
  if(auto it = st.FindMember("Process"); it != st.MemberEnd()
     && (!it->value.IsString()
         || QByteArray(it->value.GetString(), it->value.GetStringLength())
                != score::uuids::toByteArray(proc.concreteKey().impl())))
    return;

  // Loading a preset rebuilds the ports: skip it when the program is unchanged
  const rapidjson::Document current = readJson(stateBeyondControls(proc));
  if(current.IsObject() && sameProgram(st, current))
    return;

  const rapidjson::Value* controls{};
  if(auto it = st.FindMember("Controls"); it != st.MemberEnd() && it->value.IsArray())
    controls = &it->value;
  auto writeControls = [&](JsonWriter& w) {
    if(controls)
      controls->Accept(w);
    else
    {
      w.StartArray();
      w.EndArray();
    }
  };

  auto preset = proc.savePreset();
  if(auto it = st.FindMember("Script"); it != st.MemberEnd() && it->value.IsString())
  {
    preset.key.effect = QString::fromUtf8(it->value.GetString(), it->value.GetStringLength());
    JSONReader r;
    writeControls(r.stream);
    preset.data = r.toByteArray();
  }

  if(auto it = st.FindMember("Data"); it != st.MemberEnd())
  {
    JSONReader r;
    if(it->value.IsObject())
    {
      r.stream.StartObject();
      for(auto& m : it->value.GetObject())
      {
        r.stream.Key(m.name.GetString(), m.name.GetStringLength());
        m.value.Accept(r.stream);
      }
      r.stream.Key("Controls");
      writeControls(r.stream);
      r.stream.EndObject();
    }
    else
    {
      it->value.Accept(r.stream);
    }
    preset.data = r.toByteArray();
  }
  else if(auto it = st.FindMember("Text"); it != st.MemberEnd() && it->value.IsString())
  {
    preset.data = QByteArray(it->value.GetString(), it->value.GetStringLength());
  }

  // Through the preset command, so that rebuilt ports keep their cables,
  // addresses and publication
  const auto& ctx = score::IDocument::documentContext(proc);
  auto& factories = ctx.app.interfaces<LoadPresetCommandFactoryList>();
  if(auto cmd = factories.make(&LoadPresetCommandFactory::make, proc, preset, ctx))
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);
  else
    proc.loadPreset(preset);
}
}
