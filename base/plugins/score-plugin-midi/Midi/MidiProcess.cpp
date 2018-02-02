// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Midi/MidiProcess.hpp>
#include <Process/Dataflow/Port.hpp>

namespace Midi
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id,
                            Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  m_range = {59, 71};
  outlet->type = Process::PortType::Midi;

  metadata().setInstanceName(*this);

  m_device = "MidiDevice";
/*  for (int i = 0; i < 10; i++)
  {
    auto n = new Note{Id<Note>(i), this};
    n->setPitch(32 +  3 * i);
    n->setStart(0.1 + i * 0.05);
    n->setDuration(0.1 + (9 - i) * 0.05);
    n->setVelocity(i * 127. / 9.);
    notes.add(n);
  }*/
  init();
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::setDevice(const QString& dev)
{
  m_device = dev;
  emit deviceChanged(m_device);
}

const QString&ProcessModel::device() const
{
  return m_device;
}

void ProcessModel::setChannel(int n)
{
  m_channel = clamp(n, 1, 16);
  emit channelChanged(n);
}

int ProcessModel::channel() const
{
  return m_channel;
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration)
{
  setDuration(newDuration);
  emit notesChanged();
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration)
{
  auto ratio = duration() / newDuration;

  for (auto& note : notes)
    note.scale(ratio);

  setDuration(newDuration);
  emit notesChanged();
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration)
{
  auto ratio = duration() / newDuration;
  auto inv_ratio = newDuration / duration();

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
  setDuration(newDuration);
  emit notesChanged();
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
void JSONObjectReader::read(const Midi::NoteData& n)
{
  obj["Start"] = n.m_start;
  obj["Duration"] = n.m_duration;
  obj["Pitch"] = n.m_pitch;
  obj["Velocity"] = n.m_velocity;
}


template <>
void JSONObjectWriter::write(Midi::NoteData& n)
{
  n.m_start = obj["Start"].toDouble();
  n.m_duration = obj["Duration"].toDouble();
  n.m_pitch = obj["Pitch"].toInt();
  n.m_velocity = obj["Velocity"].toInt();
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
void JSONObjectReader::read(const Midi::Note& n)
{
  readFrom(n.noteData());
}


template <>
void JSONObjectWriter::write(Midi::Note& n)
{
  Midi::NoteData d;
  writeTo(d);
  n.setData(d);
}


template <>
void DataStreamReader::read(const Midi::ProcessModel& proc)
{
  m_stream << *proc.outlet <<  proc.device() << proc.channel() << proc.m_range.first << proc.m_range.second;

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
  proc.outlet = Process::make_outlet(*this, &proc);
  m_stream >> proc.m_device >> proc.m_channel  >> proc.m_range.first >> proc.m_range.second;
  int n;
  m_stream >> n;
  for (int i = 0; i < n; i++)
  {
    proc.notes.add(new Midi::Note{*this, &proc});
  }
  checkDelimiter();
}


template <>
void JSONObjectReader::read(const Midi::ProcessModel& proc)
{
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Device"] = proc.device();
  obj["Channel"] = proc.channel();
  obj["Min"] = proc.range().first;
  obj["Max"] = proc.range().second;
  obj["Notes"] = toJsonArray(proc.notes);
}


template <>
void JSONObjectWriter::write(Midi::ProcessModel& proc)
{
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);
    if(!proc.outlet)
    {
      proc.outlet = Process::make_outlet(Id<Process::Port>(0), &proc);
      proc.outlet->type = Process::PortType::Midi;
      proc.outlet->setAddress(fromJsonObject<State::AddressAccessor>(obj[strings.Address].toObject()));
    }
  }
  proc.setDevice(obj["Device"].toString());
  proc.setChannel(obj["Channel"].toInt());
  proc.setRange(obj["Min"].toInt(), obj["Max"].toInt());

  for (const auto& json_vref : obj["Notes"].toArray())
  {
    auto note = new Midi::Note{JSONObject::Deserializer{json_vref.toObject()},
                               &proc};
    proc.notes.add(note);
  }
}
