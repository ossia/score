// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MetronomeModel.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Address.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <Metronome/MetronomeProcessMetadata.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Metronome::ProcessModel)
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
    : CurveProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_value_outlet(Id<Process::Port>(0), this)}
{
  // Named shall be enough ?
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});
  m_min = 0.;
  m_max = 1.;

  auto s1 = new Curve::DefaultCurveSegmentModel(Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  init();
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel() { }

QString ProcessModel::prettyName() const noexcept
{
  return address().toString();
}

QString ProcessModel::prettyValue(double x, double y) const noexcept
{
  return QString::number((y * (max() - min()) - min()), 'f', 3);
}

void ProcessModel::init()
{
  outlet->setCustomData("Out");
  m_outlets.push_back(outlet.get());
  connect(
      outlet.get(), &Process::Port::addressChanged, this, [=](const State::AddressAccessor& arg) {
        addressChanged(arg.address);
        prettyNameChanged();
        m_curve->changed();
      });
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  // We only need to change the duration.
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
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

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
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

void ProcessModel::setCurve_impl() { }

State::Address ProcessModel::address() const
{
  return outlet->address().address;
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
  outlet->setAddress(State::AddressAccessor{arg});
}

void ProcessModel::setMin(double arg)
{
  if (m_min == arg)
    return;

  m_min = arg;
  minChanged(arg);
  m_curve->changed();
}

void ProcessModel::setMax(double arg)
{
  if (m_max == arg)
    return;

  m_max = arg;
  maxChanged(arg);
  m_curve->changed();
}
}

template <>
void DataStreamReader::read(const Metronome::ProcessModel& autom)
{
  m_stream << *autom.outlet;

  State::Address address;
  readFrom(autom.curve());

  m_stream << autom.min();
  m_stream << autom.max();

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Metronome::ProcessModel& autom)
{
  autom.outlet = Process::load_value_outlet(*this, &autom);
  autom.setCurve(new Curve::Model{*this, &autom});

  double min, max;

  m_stream >> min >> max;

  autom.setMin(min);
  autom.setMax(max);

  checkDelimiter();
}

template <>
void JSONReader::read(const Metronome::ProcessModel& autom)
{
  obj["Outlet"] = *autom.outlet;
  obj["Curve"] = autom.curve();
  obj[strings.Min] = autom.min();
  obj[strings.Max] = autom.max();
}

template <>
void JSONWriter::write(Metronome::ProcessModel& autom)
{
  JSONWriter writer{obj["Outlet"]};
  autom.outlet = Process::load_value_outlet(writer, &autom);

  JSONObject::Deserializer curve_deser{obj["Curve"]};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setMin(obj[strings.Min].toDouble());
  autom.setMax(obj[strings.Max].toDouble());
}
