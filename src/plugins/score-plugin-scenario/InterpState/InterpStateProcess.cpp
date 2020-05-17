// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterpStateProcess.hpp"

#include <Curve/Segment/Power/PowerSegment.hpp>
#include <State/ValueSerialization.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(InterpState::ProcessModel)
namespace InterpState
{
ProcessModel::~ProcessModel() = default;

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
  // Named shall be enough ?
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  metadata().setInstanceName(*this);
}

QString ProcessModel::prettyName() const noexcept
{
  return tr("State interpolation");
}

QString ProcessModel::prettyValue(double x, double y) const noexcept
{
  return QString::number(y, 'f', 3);
}

State::MessageList ProcessModel::startMessages() const
{
  return {};
}

State::MessageList ProcessModel::endMessages() const
{
  return {};
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
void DataStreamReader::read(const InterpState::ProcessModel& interp)
{
  readFrom(interp.curve());
  insertDelimiter();
}

template <>
void DataStreamWriter::write(InterpState::ProcessModel& interp)
{
  interp.setCurve(new Curve::Model{*this, &interp});
  checkDelimiter();
}

template <>
void JSONReader::read(const InterpState::ProcessModel& interp)
{
  obj["Curve"] = interp.curve();
}

template <>
void JSONWriter::write(InterpState::ProcessModel& interp)
{
  JSONObject::Deserializer curve_deser{obj["Curve"]};
  interp.setCurve(new Curve::Model{curve_deser, &interp});
}
