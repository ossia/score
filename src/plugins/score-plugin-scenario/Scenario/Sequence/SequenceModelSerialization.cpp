// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceModel.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

// -- DataStream serialization --
//
// Versioning: a int32_t version prefix is read/written before the payload.
// Bump kSequenceModelSerializationVersion when adding fields and branch on
// the version in DataStreamWriter::write(). JSON does not need a version
// because new keys are forward-compatible (missing keys read as default).
namespace
{
constexpr int32_t kSequenceModelSerializationVersion = 2;
}

template <>
void DataStreamReader::read(const Sequence::SequenceModel& seq)
{
  m_stream << kSequenceModelSerializationVersion;

  m_stream << seq.m_startTimeSyncId << seq.m_startEventId;
  m_stream << seq.m_endTimeSyncId << seq.m_endEventId;

  // Namespace (QList<State::AddressAccessor>)
  m_stream << (int32_t)seq.m_namespace.size();
  for(const auto& addr : seq.m_namespace)
    read(addr);

  // v2: parameter outlet ids (ports themselves are rebuilt from the namespace)
  m_stream << (int32_t)seq.m_paramOutlets.size();
  for(const auto& p : seq.m_paramOutlets)
    m_stream << (int32_t)p->id().val();

  // FrozenParams (QMap<Id<TimeSyncModel>, QSet<State::AddressAccessor>>)
  m_stream << (int32_t)seq.m_frozenParams.size();
  for(auto it = seq.m_frozenParams.cbegin(); it != seq.m_frozenParams.cend(); ++it)
  {
    m_stream << it.key();
    m_stream << (int32_t)it.value().size();
    for(const auto& addr : it.value())
      read(addr);
  }

  // Entity maps
  m_stream << (int32_t)seq.intervals.size();
  for(const auto& itv : seq.intervals)
    readFrom(itv);

  m_stream << (int32_t)seq.timeSyncs.size();
  for(const auto& ts : seq.timeSyncs)
    readFrom(ts);

  m_stream << (int32_t)seq.events.size();
  for(const auto& ev : seq.events)
    readFrom(ev);

  m_stream << (int32_t)seq.states.size();
  for(const auto& st : seq.states)
    readFrom(st);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Sequence::SequenceModel& seq)
{
  int32_t version = 0;
  m_stream >> version;
  SCORE_ASSERT(version >= 1 && version <= kSequenceModelSerializationVersion);

  m_stream >> seq.m_startTimeSyncId >> seq.m_startEventId;
  m_stream >> seq.m_endTimeSyncId >> seq.m_endEventId;

  // Namespace
  {
    int32_t count;
    m_stream >> count;
    seq.m_namespace.clear();
    seq.m_namespace.reserve(count);
    for(; count-- > 0;)
    {
      State::AddressAccessor addr;
      write(addr);
      seq.m_namespace.append(addr);
    }
  }

  // v2: parameter outlet ids
  {
    std::vector<int32_t> portIds;
    if(version >= 2)
    {
      int32_t count;
      m_stream >> count;
      portIds.reserve(count);
      for(; count-- > 0;)
      {
        int32_t v;
        m_stream >> v;
        portIds.push_back(v);
      }
    }
    seq.rebuildParamPorts(portIds);
  }

  // FrozenParams
  {
    int32_t mapCount;
    m_stream >> mapCount;
    for(; mapCount-- > 0;)
    {
      Id<Scenario::TimeSyncModel> tsId;
      m_stream >> tsId;
      int32_t addrCount;
      m_stream >> addrCount;
      QSet<State::AddressAccessor> addrs;
      for(; addrCount-- > 0;)
      {
        State::AddressAccessor addr;
        write(addr);
        addrs.insert(addr);
      }
      seq.m_frozenParams[tsId] = std::move(addrs);
    }
  }

  // Entity maps
  {
    int32_t count;
    m_stream >> count;
    for(; count-- > 0;)
    {
      auto itv = new Scenario::IntervalModel(*this, seq.context(), &seq);
      seq.intervals.add(itv);
    }
  }

  {
    int32_t count;
    m_stream >> count;
    for(; count-- > 0;)
    {
      auto ts = new Scenario::TimeSyncModel(*this, &seq);
      seq.timeSyncs.add(ts);
    }
  }

  {
    int32_t count;
    m_stream >> count;
    for(; count-- > 0;)
    {
      auto ev = new Scenario::EventModel(*this, &seq);
      seq.events.add(ev);
    }
  }

  {
    int32_t count;
    m_stream >> count;
    for(; count-- > 0;)
    {
      auto st = new Scenario::StateModel(*this, seq.context(), &seq);
      seq.states.add(st);
    }
  }

  // Re-wire interval↔state links
  for(const auto& itv : seq.intervals)
  {
    Scenario::SetPreviousInterval(seq.states.at(itv.endState()), itv);
    Scenario::SetNextInterval(seq.states.at(itv.startState()), itv);
  }

  seq.repairSectionRacks();

  checkDelimiter();
}

// -- JSON serialization --

template <>
void JSONReader::read(const Sequence::SequenceModel& seq)
{
  obj["StartTimeSyncId"] = seq.m_startTimeSyncId;
  obj["StartEventId"] = seq.m_startEventId;
  obj["EndTimeSyncId"] = seq.m_endTimeSyncId;
  obj["EndEventId"] = seq.m_endEventId;

  // Namespace: array of address strings
  obj["Namespace"] = seq.m_namespace;

  // Parameter outlet ids (ports themselves are rebuilt from the namespace)
  {
    stream.Key("ParamPortIds");
    stream.StartArray();
    for(const auto& p : seq.m_paramOutlets)
      stream.Int(p->id().val());
    stream.EndArray();
  }

  // FrozenParams: array of {id, addrs[]}
  {
    stream.Key("FrozenParams");
    stream.StartArray();
    for(auto it = seq.m_frozenParams.cbegin(); it != seq.m_frozenParams.cend(); ++it)
    {
      stream.StartObject();
      stream.Key("Id");
      readFrom(it.key());
      stream.Key("Addrs");
      stream.StartArray();
      for(const auto& addr : it.value())
        read(addr); // writes a JSON string
      stream.EndArray();
      stream.EndObject();
    }
    stream.EndArray();
  }

  obj["TimeNodes"] = seq.timeSyncs;
  obj["Events"] = seq.events;
  obj["States"] = seq.states;
  obj["Constraints"] = seq.intervals;
}

template <>
void JSONWriter::write(Sequence::SequenceModel& seq)
{
  seq.m_startTimeSyncId <<= obj["StartTimeSyncId"];
  seq.m_startEventId <<= obj["StartEventId"];
  seq.m_endTimeSyncId <<= obj["EndTimeSyncId"];
  seq.m_endEventId <<= obj["EndEventId"];

  // Namespace: array of address strings
  {
    JSONWriter ns_sub{obj["Namespace"]};
    ns_sub.writeTo(seq.m_namespace);
  }

  // Parameter outlet ids
  {
    std::vector<int32_t> portIds;
    if(auto val = obj.tryGet("ParamPortIds"))
    {
      for(const auto& v : val->toArray())
        portIds.push_back(v.GetInt());
    }
    seq.rebuildParamPorts(portIds);
  }

  // FrozenParams: array of {id, addrs[]}
  {
    for(const auto& entry : obj["FrozenParams"].toArray())
    {
      const auto& entryObj = entry.GetObject();

      Id<Scenario::TimeSyncModel> tsId;
      {
        JSONWriter id_sub{entryObj["Id"]};
        id_sub.writeTo(tsId);
      }

      QSet<State::AddressAccessor> addrs;
      for(const auto& addrVal : entryObj["Addrs"].GetArray())
      {
        State::AddressAccessor addr;
        JSONWriter addr_sub{addrVal};
        addr_sub.write(addr);
        addrs.insert(addr);
      }
      seq.m_frozenParams[tsId] = std::move(addrs);
    }
  }

  // Entity maps
  for(const auto& json_vref : obj["Constraints"].toArray())
  {
    auto itv = new Scenario::IntervalModel(
        JSONObject::Deserializer{json_vref}, seq.context(), &seq);
    seq.intervals.add(itv);
  }

  for(const auto& json_vref : obj["TimeNodes"].toArray())
  {
    auto ts = new Scenario::TimeSyncModel(JSONObject::Deserializer{json_vref}, &seq);
    seq.timeSyncs.add(ts);
  }

  for(const auto& json_vref : obj["Events"].toArray())
  {
    auto ev = new Scenario::EventModel(JSONObject::Deserializer{json_vref}, &seq);
    seq.events.add(ev);
  }

  for(const auto& json_vref : obj["States"].toArray())
  {
    auto st = new Scenario::StateModel(
        JSONObject::Deserializer{json_vref}, seq.context(), &seq);
    seq.states.add(st);
  }

  // Re-wire interval↔state links
  for(const auto& itv : seq.intervals)
  {
    Scenario::SetPreviousInterval(seq.states.at(itv.endState()), itv);
    Scenario::SetNextInterval(seq.states.at(itv.startState()), itv);
  }

  seq.repairSectionRacks();
}
