// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationModel.hpp"

#include <State/Address.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/MinMaxFloatPort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PrettyPortName.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/State/AutomationState.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Automation::ProcessModel)

namespace Automation
{

void ProcessModel::init()
{
  outlet->setName("Out");
  auto& out = *safe_cast<Process::MinMaxFloatOutlet*>(outlet.get());
  m_outlets.push_back(&out);
  connect(
      &out, &Process::Port::addressChanged, this,
      [=](const State::AddressAccessor& arg) {
    addressChanged(arg);
    prettyNameChanged();
    unitChanged(arg.qualifiers.get().unit);
    m_curve->changed();
      });
  connect(&out, &Process::Port::cablesChanged, this, [=] { prettyNameChanged(); });

  connect(
      out.minInlet.get(), &Process::FloatSlider::valueChanged, this,
      [=](const ossia::value& arg) { minChanged(ossia::convert<float>(arg)); });
  connect(
      out.maxInlet.get(), &Process::FloatSlider::valueChanged, this,
      [=](const ossia::value& arg) { maxChanged(ossia::convert<float>(arg)); });
}

ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : CurveProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{std::make_unique<Process::MinMaxFloatOutlet>(Id<Process::Port>(0), this)}
    , m_startState{new ProcessState{*this, 0., this}}
    , m_endState{new ProcessState{*this, 1., this}}
{
  // Named shall be enough ?
  setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

  auto s1 = new Curve::DefaultCurveSegmentModel(Id<Curve::SegmentModel>(1), m_curve);
  s1->setStart({0., 0.0});
  s1->setEnd({1., 1.});

  m_curve->addSegment(s1);

  metadata().setInstanceName(*this);

  init();
}

ProcessModel::~ProcessModel() { }

ProcessModel::ProcessModel(JSONObject::Deserializer& vis, QObject* parent)
    : CurveProcessModel{vis, parent}
    , m_startState{new ProcessState{*this, 0., this}}
    , m_endState{new ProcessState{*this, 1., this}}
{
  vis.writeTo(*this);

  auto out = dynamic_cast<Process::MinMaxFloatOutlet*>(outlet.get());
  if(!out)
  {
    if(outlet.get())
    {
      auto new_out
          = std::make_unique<Process::MinMaxFloatOutlet>(Id<Process::Port>(0), this);
      new_out->takeCables(std::move(*outlet));
      new_out->setSettings(outlet->settings());
      this->m_outlets.clear();
      auto p = new_out.get();
      outlet = std::move(new_out);
      this->m_outlets.push_back(p);
      this->outletsChanged();
    }
  }
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

QString ProcessModel::prettyName() const noexcept
{
  auto& doc = score::IDocument::documentContext(*this);
  if(auto name = Process::displayNameForPort(*outlet, doc); !name.isEmpty())
  {
    return name;
  }

  return QStringLiteral("Automation");
}

QString ProcessModel::prettyValue(double x, double y) const noexcept
{
  return QString::number((y * (max() - min()) + min()), 'f', 3);
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
  if(m_curve->segments().size() == 0)
  {
    setDuration(newDuration);
    return;
  }

  // Else, scale all the segments by the increase.
  double scale = duration() / newDuration;
  for(auto& segment : m_curve->segments())
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
  if(m_curve->segments().size() == 0)
  {
    setDuration(newDuration);
    return;
  }

  if(newDuration <= TimeVal::zero())
  {
    setDuration(TimeVal::zero());
    m_curve->clear();
    return;
  }

  // Else, scale all the segments by the increase.
  double scale = duration() / newDuration;
  for(auto& segment : m_curve->segments())
  {
    Curve::Point pt = segment.start();
    pt.setX(pt.x() * scale);
    segment.setStart(pt);

    pt = segment.end();
    pt.setX(pt.x() * scale);
    segment.setEnd(pt);
  }
  //
  //    // Since we shrink, scale > 1. so we have to cut.
  //    // Note:  this will certainly change how some functions do look.
  //    auto segments = shallow_copy(m_curve->segments());// Make a copy since we
  //   will change the map.
  //    for(auto segment : segments)
  //    {
  //        if(segment->start().x() >= 1.)
  //        {
  //            // bye
  //            m_curve->removeSegment(segment);
  //        }
  //        else if(segment->end().x() >= 1.)
  //        {
  //            auto end = segment->end();
  //            end.setX(1.);
  //            segment->setEnd(end);
  //        }
  //    }
  //
  setDuration(newDuration);
  m_curve->changed();
}

void ProcessModel::setCurve_impl()
{
  return;
  connect(m_curve, &Curve::Model::changed, this, [&]() {
    m_startState->messagesChanged(m_startState->messages());
    m_endState->messagesChanged(m_endState->messages());
  });
}

ProcessState* ProcessModel::startStateData() const noexcept
{
  return m_startState;
}

ProcessState* ProcessModel::endStateData() const noexcept
{
  return m_endState;
}

const ::State::AddressAccessor& ProcessModel::address() const
{
  return outlet->address();
}

double ProcessModel::min() const
{
  return ossia::convert<float>(
      ((Process::MinMaxFloatOutlet&)(*outlet)).minInlet->value());
}

double ProcessModel::max() const
{
  return ossia::convert<float>(
      ((Process::MinMaxFloatOutlet&)(*outlet)).maxInlet->value());
}

void ProcessModel::setAddress(const ::State::AddressAccessor& arg)
{
  outlet->setAddress(arg);
}

void ProcessModel::setMin(double arg)
{
  auto& inlet = ((Process::MinMaxFloatOutlet&)(*outlet)).minInlet;
  if(ossia::convert<float>(inlet->value()) == arg)
    return;

  inlet->setValue(arg);
  minChanged(arg);
  m_curve->changed();
}

void ProcessModel::setMax(double arg)
{
  auto& inlet = ((Process::MinMaxFloatOutlet&)(*outlet)).maxInlet;
  if(ossia::convert<float>(inlet->value()) == arg)
    return;

  inlet->setValue(arg);
  maxChanged(arg);
  m_curve->changed();
}

State::Unit ProcessModel::unit() const
{
  return outlet->address().qualifiers.get().unit;
}

void ProcessModel::setUnit(const State::Unit& u)
{
  if(u != unit())
  {
    auto addr = outlet->address();
    addr.qualifiers.get().unit = u;
    outlet->setAddress(addr);
    prettyNameChanged();
    unitChanged(u);
  }
}

bool ProcessModel::tween() const
{
  return m_tween;
}

void ProcessModel::setTween(bool tween)
{
  if(m_tween == tween)
    return;

  m_tween = tween;
  tweenChanged(tween);
}

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  m_curve->clear();
  JSONWriter{readJson(preset.data)}.write(*m_curve);
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key = {this->concreteKey(), {}};

  JSONReader r;
  r.stream.StartObject();
  r.read(*m_curve);
  r.stream.EndObject();

  p.data = r.toByteArray();
  return p;
}
}
