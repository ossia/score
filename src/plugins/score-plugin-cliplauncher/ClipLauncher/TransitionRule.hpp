#pragma once
#include <ClipLauncher/Types.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <cstdint>

namespace ClipLauncher
{

struct TransitionTarget
{
  int lane{-1};  // -1 = same lane
  int scene{-1}; // -1 = next scene, -2 = random, >= 0 = specific scene
};

struct TransitionRule
{
  enum class Condition : uint8_t
  {
    OnEnd,          // When clip reaches its end
    AfterLoopCount, // After N loops
    OnTrigger,      // On external trigger
    Probability     // Random chance per loop
  };

  int32_t id{};
  Condition condition{Condition::OnEnd};
  int loopCount{1};
  double probability{1.0};
  int priority{0};

  TransitionTarget target;
  LaunchMode launchMode{LaunchMode::Immediate};
};

} // namespace ClipLauncher

template <>
struct is_custom_serialized<ClipLauncher::TransitionTarget> : std::true_type
{
};
template <>
struct is_custom_serialized<ClipLauncher::TransitionRule> : std::true_type
{
};

// DataStream serialization for TransitionTarget
template <>
struct TSerializer<DataStream, ClipLauncher::TransitionTarget>
{
  static void readFrom(DataStream::Serializer& s, const ClipLauncher::TransitionTarget& v)
  {
    s.stream() << v.lane << v.scene;
  }
  static void writeTo(DataStream::Deserializer& s, ClipLauncher::TransitionTarget& v)
  {
    s.stream() >> v.lane >> v.scene;
  }
};

// DataStream serialization for TransitionRule
template <>
struct TSerializer<DataStream, ClipLauncher::TransitionRule>
{
  static void readFrom(DataStream::Serializer& s, const ClipLauncher::TransitionRule& v)
  {
    s.stream() << v.id << static_cast<uint8_t>(v.condition) << v.loopCount
               << v.probability << v.priority << v.target
               << static_cast<uint8_t>(v.launchMode);
  }
  static void writeTo(DataStream::Deserializer& s, ClipLauncher::TransitionRule& v)
  {
    uint8_t cond, mode;
    s.stream() >> v.id >> cond >> v.loopCount >> v.probability >> v.priority >> v.target
        >> mode;
    v.condition = static_cast<ClipLauncher::TransitionRule::Condition>(cond);
    v.launchMode = static_cast<ClipLauncher::LaunchMode>(mode);
  }
};

// JSON serialization for TransitionTarget
template <>
struct TSerializer<JSONObject, ClipLauncher::TransitionTarget>
{
  static void
  readFrom(JSONObject::Serializer& s, const ClipLauncher::TransitionTarget& v)
  {
    s.stream.StartObject();
    s.obj["Lane"] = v.lane;
    s.obj["Scene"] = v.scene;
    s.stream.EndObject();
  }
  static void writeTo(JSONObject::Deserializer& s, ClipLauncher::TransitionTarget& v)
  {
    v.lane = s.obj["Lane"].toInt();
    v.scene = s.obj["Scene"].toInt();
  }
};

// JSON serialization for TransitionRule
template <>
struct TSerializer<JSONObject, ClipLauncher::TransitionRule>
{
  static void readFrom(JSONObject::Serializer& s, const ClipLauncher::TransitionRule& v)
  {
    s.stream.StartObject();
    s.obj["Id"] = v.id;
    s.obj["Condition"] = static_cast<int>(v.condition);
    s.obj["LoopCount"] = v.loopCount;
    s.obj["Probability"] = v.probability;
    s.obj["Priority"] = v.priority;
    s.obj["Target"] = v.target;
    s.obj["LaunchMode"] = static_cast<int>(v.launchMode);
    s.stream.EndObject();
  }
  static void writeTo(JSONObject::Deserializer& s, ClipLauncher::TransitionRule& v)
  {
    v.id = s.obj["Id"].toInt();
    v.condition
        = static_cast<ClipLauncher::TransitionRule::Condition>(s.obj["Condition"].toInt());
    v.loopCount = s.obj["LoopCount"].toInt();
    v.probability = s.obj["Probability"].toDouble();
    v.priority = s.obj["Priority"].toInt();
    {
      JSONObject::Deserializer sub{s.obj["Target"]};
      sub.writeTo(v.target);
    }
    v.launchMode = static_cast<ClipLauncher::LaunchMode>(s.obj["LaunchMode"].toInt());
  }
};
