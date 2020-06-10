// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Midi/MidiProcess.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/model/EntityMapSerialization.hpp>

#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Midi::ProcessModel)

namespace Midi
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_midi_outlet(Id<Process::Port>(0), this)}
{
  m_range = {60, 71};

  metadata().setInstanceName(*this);
  init();
}

void ProcessModel::init()
{
  m_outlets.push_back(outlet.get());
}

ProcessModel::~ProcessModel() { }

void ProcessModel::setChannel(int n)
{
  n = std::clamp(n, 1, 16);
  if (n != m_channel)
  {
    m_channel = n;
    channelChanged(n);
  }
}

int ProcessModel::channel() const
{
  return m_channel;
}

void ProcessModel::setRange(int min, int max)
{
  if (min == max)
  {
    min = 127;
    max = 0;

    for (auto& note : notes)
    {
      if (note.pitch() < min)
        min = note.pitch();
      if (note.pitch() > max)
        max = note.pitch();
    }
  }
  else
  {
    min = std::max(0, min);
    max = std::min(127, max);
    if (min >= max)
    {
      min = std::max(0, max - 7);
      max = std::min(127, min + 14);
    }
  }

  std::pair<int, int> range{min, max};
  if (range != m_range)
  {
    m_range = range;
    rangeChanged(min, max);
  }
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  notesChanged();
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  if(duration() == newDuration)
    return;
  if(newDuration == TimeVal::zero())
    return;
  auto ratio = double(duration().impl) / newDuration.impl;

  for (auto& note : notes)
    note.scale(ratio);

  notesChanged();
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  if(duration() == newDuration)
    return;
  if(newDuration == TimeVal::zero())
    return;

  auto ratio = double(duration().impl) / newDuration.impl;
  auto inv_ratio = newDuration.impl / double(duration().impl);

  std::vector<Id<Note>> toErase;
  for (Note& n : notes)
  {
    if (n.end() >= inv_ratio)
    {
      toErase.push_back(n.id());
    }
    else
    {
      n.scale(ratio);
    }
  }

  for (auto& note : toErase)
  {
    notes.remove(note);
  }
  notesChanged();
  setDuration(newDuration);
}
}

template <>
void DataStreamReader::read(const Midi::NoteData& n)
{
  m_stream << n.m_start << n.m_duration << n.m_pitch << n.m_velocity;
}

template <>
void DataStreamWriter::write(Midi::NoteData& n)
{
  m_stream >> n.m_start >> n.m_duration >> n.m_pitch >> n.m_velocity;
}

template <>
void JSONReader::read(const Midi::NoteData& n)
{
  stream.StartArray();
  stream.Double(n.m_start);
  stream.Double(n.m_duration);
  stream.Int(n.m_pitch);
  stream.Int(n.m_velocity);
  stream.EndArray();
}

template <>
void JSONWriter::write(Midi::NoteData& n)
{
  const auto& arr = base.GetArray();
  n.m_start = arr[0].GetDouble();
  n.m_duration = arr[1].GetDouble();
  n.m_pitch = arr[2].GetInt();
  n.m_velocity = arr[3].GetInt();
}

template <>
void DataStreamReader::read(const Midi::Note& n)
{
  m_stream << n.noteData();
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Midi::Note& n)
{
  Midi::NoteData d;
  m_stream >> d;
  n.setData(d);
  checkDelimiter();
}

template <>
void JSONReader::read(const Midi::Note& n)
{
  stream.Key("Note");
  stream.StartArray();
  stream.Double(n.m_start);
  stream.Double(n.m_duration);
  stream.Int(n.m_pitch);
  stream.Int(n.m_velocity);
  stream.EndArray();
}

template <>
void JSONWriter::write(Midi::Note& n)
{
  const auto& arr = obj["Note"].toArray();
  n.m_start = arr[0].GetDouble();
  n.m_duration = arr[1].GetDouble();
  n.m_pitch = arr[2].GetInt();
  n.m_velocity = arr[3].GetInt();
}

template <>
void DataStreamReader::read(const Midi::ProcessModel& proc)
{
  m_stream << *proc.outlet << proc.channel() << proc.m_range.first << proc.m_range.second;

  const auto& notes = proc.notes;

  m_stream << (int32_t)notes.size();
  for (const auto& n : notes)
  {
    readFrom(n);
  }

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Midi::ProcessModel& proc)
{
  proc.outlet = Process::load_midi_outlet(*this, &proc);
  m_stream >> proc.m_channel >> proc.m_range.first >> proc.m_range.second;
  int n;
  m_stream >> n;
  for (int i = 0; i < n; i++)
  {
    proc.notes.add(new Midi::Note{*this, &proc});
  }
  checkDelimiter();
}

template <>
void JSONReader::read(const Midi::ProcessModel& proc)
{
  obj["Outlet"] = *proc.outlet;
  obj["Channel"] = proc.channel();
  obj["Min"] = proc.range().first;
  obj["Max"] = proc.range().second;
  obj["Notes"] = proc.notes;
}

template <>
void JSONWriter::write(Midi::ProcessModel& proc)
{
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_midi_outlet(writer, &proc);
  }

  for (const auto& json_vref : obj["Notes"].toArray())
  {
    auto note = new Midi::Note{JSONObject::Deserializer{json_vref}, &proc};
    proc.notes.add(note);
  }

  proc.setChannel(obj["Channel"].toInt());
  proc.setRange(obj["Min"].toInt(), obj["Max"].toInt());
}
