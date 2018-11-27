// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterpolationProcess.hpp"

#include <Curve/Segment/Power/PowerSegment.hpp>
#include <State/ValueSerialization.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Interpolation::ProcessModel)
namespace Interpolation
{
ProcessModel::~ProcessModel() = default;

bool ProcessModel::contentHasDuration() const noexcept
{
  return true;
}

TimeVal ProcessModel::contentDuration() const noexcept
{
  return duration() * std::min(1., m_curve->lastPointPos());
}

QString ProcessModel::prettyValue(double x, double y) const noexcept
{
  return QString::number(y, 'f', 3);
}

ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
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
  addressChanged(arg);
  prettyNameChanged();
  m_curve->changed();
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
  startChanged(arg);
  m_curve->changed();
}

void ProcessModel::setEnd(ossia::value arg)
{
  if (m_end == arg)
    return;

  m_end = arg;
  endChanged(arg);
  m_curve->changed();
}

QString ProcessModel::prettyName() const noexcept
{
  return address().toString();
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

}

template <>
void DataStreamReader::read(const Interpolation::ProcessModel& interp)
{
  readFrom(interp.curve());

  m_stream << interp.address() << interp.sourceUnit() << interp.start()
           << interp.end() << interp.tween();

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Interpolation::ProcessModel& interp)
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

template <>
void JSONObjectReader::read(const Interpolation::ProcessModel& interp)
{
  obj["Curve"] = toJsonObject(interp.curve());
  obj[strings.Address] = toJsonObject(interp.address());
  obj[strings.Unit] = QString::fromStdString(
      ossia::get_pretty_unit_text(interp.sourceUnit()));
  obj[strings.Start] = toJsonObject(interp.start());
  obj[strings.End] = toJsonObject(interp.end());
  obj["Tween"] = interp.tween();
}

template <>
void JSONObjectWriter::write(Interpolation::ProcessModel& interp)
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
