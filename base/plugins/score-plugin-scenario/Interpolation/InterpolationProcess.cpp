// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterpolationProcess.hpp"
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Interpolation
{
ProcessModel::~ProcessModel() = default;

ProcessState::ProcessState(
    ProcessModel& model, Point watchedPoint, QObject* parent)
    : ProcessStateDataInterface{model, parent}, m_point{watchedPoint}
{
}

ProcessModel& ProcessState::process() const
{
  return static_cast<ProcessModel&>(ProcessStateDataInterface::process());
}

State::Message ProcessState::message() const
{
  auto& proc = process();

  if (m_point == Point::Start)
  {
    return State::Message{proc.address(), proc.start()};
  }
  else if (m_point == Point::End)
  {
    return State::Message{proc.address(), proc.end()};
  }

  return {};
}

ProcessState::Point ProcessState::point() const
{
  return m_point;
}

std::vector<State::AddressAccessor> ProcessState::matchingAddresses()
{
  // TODO have a better check of "address validity"
  auto addr = process().address();
  if (!addr.address.device.isEmpty())
    return {std::move(addr)};
  return {};
}

bool ProcessModel::contentHasDuration() const
{
  return true;
}

TimeVal ProcessModel::contentDuration() const
{
  return duration() * std::min(1., m_curve->lastPointPos());
}

State::MessageList ProcessState::messages() const
{
  /*
  if(!process().address().address.device.isEmpty())
  {
      auto mess = message();
      if(!mess.address.address.device.isEmpty())
          return {mess};
  }
  */

  return {};
}

State::MessageList ProcessState::setMessages(
    const State::MessageList& received, const Process::MessageNode&)
{
  auto& proc = process();
  State::AddressAccessor cur_address = proc.address();
  auto it = ossia::find_if(received, [&](const auto& mess) {
    return mess.address.address == cur_address.address
           && mess.address.qualifiers.get().accessors
                  == cur_address.qualifiers.get().accessors;
    // The unit is handled later.
  });
  if (it != received.end())
  {
    if (m_point == Point::Start)
    {
      proc.setStart(it->value);
    }
    else if (m_point == Point::End)
    {
      proc.setEnd(it->value);
    }
    proc.setSourceUnit(it->address.qualifiers.get().unit);
  }
  return messages();
}

ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , m_startState{new ProcessState{*this, ProcessState::Start, this}}
    , m_endState{new ProcessState{*this, ProcessState::End, this}}
{
  // Named shall be enough ?
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(
      Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  metadata().setInstanceName(*this);
}

State::AddressAccessor ProcessModel::address() const
{
  return m_address;
}

const State::Unit& ProcessModel::sourceUnit() const
{
  return m_sourceUnit;
}

ossia::value ProcessModel::start() const
{
  return m_start;
}

ossia::value ProcessModel::end() const
{
  return m_end;
}

void ProcessModel::setAddress(const State::AddressAccessor& arg)
{
  if (m_address == arg)
  {
    return;
  }

  m_address = arg;
  emit addressChanged(arg);
  emit prettyNameChanged();
  emit m_curve->changed();
}

void ProcessModel::setSourceUnit(const State::Unit& u)
{
  m_sourceUnit = u;
}

void ProcessModel::setStart(ossia::value arg)
{
  if (m_start == arg)
    return;

  m_start = arg;
  emit startChanged(arg);
  emit m_curve->changed();
}

void ProcessModel::setEnd(ossia::value arg)
{
  if (m_end == arg)
    return;

  m_end = arg;
  emit endChanged(arg);
  emit m_curve->changed();
}

QString ProcessModel::prettyName() const
{
  return address().toString();
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration)
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

ProcessState* ProcessModel::startStateData() const
{
  return m_startState;
}

ProcessState* ProcessModel::endStateData() const
{
  return m_endState;
}

}


template <> void
DataStreamReader::read(
    const Interpolation::ProcessModel& interp)
{
  readFrom(interp.curve());

  m_stream << interp.address() << interp.sourceUnit() << interp.start()
           << interp.end() << interp.tween();

  insertDelimiter();
}


template <> void
DataStreamWriter::write(Interpolation::ProcessModel& interp)
{
  interp.setCurve(new Curve::Model{*this, &interp});

  State::AddressAccessor address;
  ossia::unit_t u;
  ossia::value start, end;
  bool tw;

  m_stream >> address >> u >> start >> end >> tw;

  interp.setAddress(address);
  interp.setSourceUnit(u);
  interp.setStart(start);
  interp.setEnd(end);
  interp.setTween(tw);

  checkDelimiter();
}


template <> void
JSONObjectReader::read(
    const Interpolation::ProcessModel& interp)
{
  obj["Curve"] = toJsonObject(interp.curve());
  obj[strings.Address] = toJsonObject(interp.address());
  obj[strings.Unit] = QString::fromStdString(
      ossia::get_pretty_unit_text(interp.sourceUnit()));
  obj[strings.Start] = toJsonObject(interp.start());
  obj[strings.End] = toJsonObject(interp.end());
  obj["Tween"] = interp.tween();
}


template <> void
JSONObjectWriter::write(Interpolation::ProcessModel& interp)
{
  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  interp.setCurve(new Curve::Model{curve_deser, &interp});

  interp.setAddress(
      fromJsonObject<State::AddressAccessor>(obj[strings.Address]));
  interp.setSourceUnit(
      ossia::parse_pretty_unit(obj[strings.Unit].toString().toStdString()));
  interp.setStart(fromJsonObject<ossia::value>(obj[strings.Start]));
  interp.setEnd(fromJsonObject<ossia::value>(obj[strings.End]));
  interp.setTween(obj["Tween"].toBool());
}
