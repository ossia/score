// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Patternist/PatternModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Patternist::ProcessModel)

template <>
struct TSerializer<
    DataStream,
    std::vector<bool>,
    std::enable_if_t<is_QDataStreamSerializable<bool>::value>>
{
  static void
  readFrom(DataStream::Serializer& s, const std::vector<bool>& vec)
  {
    s.stream() << (int32_t)vec.size();
    for (bool elt : vec)
      s.stream() << elt;

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, std::vector<bool>& vec)
  {
    int32_t n = 0;
    s.stream() >> n;

    vec.clear();
    vec.resize(n);
    for (int32_t i = 0; i < n; i++)
    {
      bool b;
      s.stream() >> b;
      vec[i] = b;
    }

    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};
namespace Patternist
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  outlet->type = Process::PortType::Midi;

  Pattern pattern;
  pattern.lanes.push_back(Lane{
                            64,
                            {1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0}
                          });
  pattern.lanes.push_back(Lane{
                            32,
                            {0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0}
                          });
  m_patterns.push_back(pattern);
  metadata().setInstanceName(*this);
  init();
}

ProcessModel::~ProcessModel() {}

void ProcessModel::setChannel(int n)
{
  n = std::clamp(n, 1, 16);
  if(n != m_channel)
  {
    m_channel = n;
    channelChanged(n);
  }
}

int ProcessModel::channel() const noexcept
{
  return m_channel;
}

void ProcessModel::setCurrentPattern(int n)
{
  n = std::clamp(n, 0, (int)m_patterns.size());
  if(n != m_currentPattern)
  {
    m_currentPattern = n;
    currentPatternChanged(n);
  }
}

int ProcessModel::currentPattern() const noexcept
{
  return m_currentPattern;
}


void ProcessModel::setPatterns(const std::vector<Pattern>& n)
{
  if(n != m_patterns)
  {
    m_patterns = n;
    patternsChanged();
  }
}

const std::vector<Pattern>& ProcessModel::patterns() const noexcept
{
  return m_patterns;
}


void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}
}

template <>
void DataStreamReader::read(const Patternist::Lane& proc)
{
  m_stream << proc.note << proc.pattern;
}

template <>
void DataStreamWriter::write(Patternist::Lane& proc)
{
  m_stream >> proc.note >> proc.pattern;
}

template <>
void JSONObjectReader::read(const Patternist::Lane& proc)
{
  obj["Note"] = proc.note;
  QJsonArray arr;
  for(bool b : proc.pattern)
    arr.push_back(b ? 1 : 0);
  obj["Pattern"] = arr;
}

template <>
void JSONObjectWriter::write(Patternist::Lane& proc)
{
  proc.note = obj["Note"].toInt();
  for(QJsonValue val : obj["Pattern"].toArray())
    proc.pattern.push_back(val.toInt() == 1);
}



template <>
void DataStreamReader::read(const Patternist::Pattern& proc)
{
  m_stream << proc.length << proc.division << proc.lanes;
}

template <>
void DataStreamWriter::write(Patternist::Pattern& proc)
{
  m_stream >> proc.length >> proc.division >> proc.lanes;
}

template <>
void JSONObjectReader::read(const Patternist::Pattern& proc)
{
  obj["Length"] = proc.length;
  obj["Division"] = proc.division;
  obj["Lanes"] = toJsonArray(proc.lanes);
}

template <>
void JSONObjectWriter::write(Patternist::Pattern& proc)
{
  proc.length = obj["Length"].toInt();
  proc.division = obj["Division"].toInt();
  fromJsonArray(obj["Lanes"].toArray(), proc.lanes);
}


template <>
void DataStreamReader::read(const Patternist::ProcessModel& proc)
{
  m_stream << *proc.outlet
           << proc.m_channel << proc.m_currentPattern << proc.m_patterns;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Patternist::ProcessModel& proc)
{
  proc.outlet = Process::make_outlet(*this, &proc);
  m_stream >> proc.m_channel >> proc.m_currentPattern >> proc.m_patterns;

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Patternist::ProcessModel& proc)
{
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Channel"] = proc.m_channel;
  obj["Pattern"] = proc.m_currentPattern;
  obj["Patterns"] = toJsonArray(proc.m_patterns);
}

template <>
void JSONObjectWriter::write(Patternist::ProcessModel& proc)
{
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
  }
  proc.m_channel = obj["Channel"].toInt();
  proc.m_currentPattern = obj["Pattern"].toInt();
  fromJsonArray(obj["Patterns"].toArray(), proc.m_patterns);
}
