// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QDebug>
#include <QPoint>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include "AutomationModel.hpp"
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Automation/AutomationProcessMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Automation/State/AutomationState.hpp>
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


namespace Process
{
class ProcessModel;
}
class QObject;
namespace Automation
{
Process::Inlets ProcessModel::inlets() const
{
  return {};
}

Process::Outlets ProcessModel::outlets() const
{
  return {outlet.get()};
}

void ProcessModel::init()
{
  connect(outlet.get(), &Process::Port::addressChanged,
          this, [=] (const State::AddressAccessor& arg) {
    emit addressChanged(arg);
    emit prettyNameChanged();
    emit unitChanged(arg.qualifiers.get().unit);
    emit m_curve->changed();
  });
}

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{duration, id,
                        Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
    , m_min{0.}
    , m_max{1.}
    , m_startState{new ProcessState{*this, 0., this}}
    , m_endState{new ProcessState{*this, 1., this}}
{
  outlet->type = Process::PortType::Message;

  // Named shall be enough ?
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(
      Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  metadata().setInstanceName(*this);
  init();
}

ProcessModel::~ProcessModel()
{
}

ProcessModel::ProcessModel(JSONObject::Deserializer& vis, QObject* parent)
  : CurveProcessModel{vis, parent}
  , m_startState{new ProcessState{*this, 0., this}}
  , m_endState{new ProcessState{*this, 1., this}}
{
  vis.writeTo(*this);
  init();
}

ProcessModel::ProcessModel(DataStream::Deserializer& vis, QObject* parent)
  : CurveProcessModel{vis, parent}
  , m_startState{new ProcessState{*this, 0., this}}
  , m_endState{new ProcessState{*this, 1., this}}
{
  vis.writeTo(*this);
  init();
}


QString ProcessModel::prettyName() const
{
  auto res = address().toString();
  if(!res.isEmpty())
    return res;
  return "Automation";
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
  connect(m_curve, &Curve::Model::changed, this, [&]() {
    m_startState->messagesChanged(m_startState->messages());
    m_endState->messagesChanged(m_endState->messages());
  });
}

ProcessState* ProcessModel::startStateData() const
{
  return m_startState;
}

ProcessState* ProcessModel::endStateData() const
{
  return m_endState;
}

const ::State::AddressAccessor& ProcessModel::address() const
{
  return outlet->address();
}

double ProcessModel::min() const
{
  return m_min;
}

double ProcessModel::max() const
{
  return m_max;
}

void ProcessModel::setAddress(const ::State::AddressAccessor& arg)
{
  outlet->setAddress(arg);
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

State::Unit ProcessModel::unit() const
{
  return outlet->address().qualifiers.get().unit;
}

void ProcessModel::setUnit(const State::Unit& u)
{
  if (u != unit())
  {
    auto addr = outlet->address();
    addr.qualifiers.get().unit = u;
    outlet->setAddress(addr);
    emit prettyNameChanged();
    emit unitChanged(u);
  }
}
}
