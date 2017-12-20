// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QDebug>
#include <QPoint>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include "MetronomeModel.hpp"
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Automation/Metronome/MetronomeProcessMetadata.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <State/Address.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/model/Identifier.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include "MetronomeModel.hpp"
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

namespace Process
{
class ProcessModel;
}
class QObject;
namespace Metronome
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
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

ProcessModel::~ProcessModel()
{
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
  // If there are no segments, nothing changes
  if (m_curve->segments().size() == 0)
  {
    setDuration(newDuration);
    return;
  }

  // Else, scale all the segments by the increase.
  double scale = duration() / newDuration;
  for (auto& segment : m_curve->segments())
  {
    Curve::Point pt = segment.start();
    pt.setX(pt.x() * scale);
    segment.setStart(pt);

    pt = segment.end();
    pt.setX(pt.x() * scale);
    segment.setEnd(pt);
  }

  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration)
{
  // If there are no segments, nothing changes
  if (m_curve->segments().size() == 0)
  {
    setDuration(newDuration);
    return;
  }

  // Else, scale all the segments by the increase.
  double scale = duration() / newDuration;
  for (auto& segment : m_curve->segments())
  {
    Curve::Point pt = segment.start();
    pt.setX(pt.x() * scale);
    segment.setStart(pt);

    pt = segment.end();
    pt.setX(pt.x() * scale);
    segment.setEnd(pt);
  }
  /*
      // Since we shrink, scale > 1. so we have to cut.
      // Note:  this will certainly change how some functions do look.
      auto segments = shallow_copy(m_curve->segments());// Make a copy since we
     will change the map.
      for(auto segment : segments)
      {
          if(segment->start().x() >= 1.)
          {
              // bye
              m_curve->removeSegment(segment);
          }
          else if(segment->end().x() >= 1.)
          {
              auto end = segment->end();
              end.setX(1.);
              segment->setEnd(end);
          }
      }
  */
  setDuration(newDuration);
  m_curve->changed();
}

bool ProcessModel::contentHasDuration() const
{
  return true;
}

TimeVal ProcessModel::contentDuration() const
{
  return duration() * std::min(1., m_curve->lastPointPos());
}

void ProcessModel::setCurve_impl()
{
}

State::Address ProcessModel::address() const
{
  return m_address;
}

double ProcessModel::min() const
{
  return m_min;
}

double ProcessModel::max() const
{
  return m_max;
}

void ProcessModel::setAddress(const State::Address& arg)
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

void ProcessModel::setMin(double arg)
{
  if (m_min == arg)
    return;

  m_min = arg;
  emit minChanged(arg);
  emit m_curve->changed();
}

void ProcessModel::setMax(double arg)
{
  if (m_max == arg)
    return;

  m_max = arg;
  emit maxChanged(arg);
  emit m_curve->changed();
}

}


template <>
void DataStreamReader::read(
    const Metronome::ProcessModel& autom)
{
  readFrom(autom.curve());

  m_stream << autom.address();
  m_stream << autom.min();
  m_stream << autom.max();

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Metronome::ProcessModel& autom)
{
  autom.setCurve(new Curve::Model{*this, &autom});

  State::Address address;
  double min, max;

  m_stream >> address >> min >> max;

  autom.setAddress(address);
  autom.setMin(min);
  autom.setMax(max);

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Metronome::ProcessModel& autom)
{
  obj["Curve"] = toJsonObject(autom.curve());
  obj[strings.Address] = toJsonObject(autom.address());
  obj[strings.Min] = autom.min();
  obj[strings.Max] = autom.max();
}


template <>
void JSONObjectWriter::write(Metronome::ProcessModel& autom)
{
  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setAddress(
      fromJsonObject<State::Address>(obj[strings.Address].toObject()));
  autom.setMin(obj[strings.Min].toDouble());
  autom.setMax(obj[strings.Max].toDouble());
}

