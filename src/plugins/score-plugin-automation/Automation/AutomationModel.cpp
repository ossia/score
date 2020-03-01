// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationModel.hpp"

#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/State/AutomationState.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <State/Address.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/MapCopy.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/dataflow/port.hpp>

namespace ossia
{
class minmax_float_outlet : public value_outlet
{
public:
  minmax_float_outlet()
  {
    this->child_inlets.push_back(&min_inlet);
    this->child_inlets.push_back(&max_inlet);
  }

  ossia::value_inlet min_inlet;
  ossia::value_inlet max_inlet;
};
}
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT MinMaxFloatOutlet : public ValueOutlet
{
  W_OBJECT(MinMaxFloatOutlet)

SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(MinMaxFloatOutlet)
  MinMaxFloatOutlet() = delete;
  ~MinMaxFloatOutlet() override;
  MinMaxFloatOutlet(const MinMaxFloatOutlet&) = delete;
  MinMaxFloatOutlet(Id<Process::Port> c, QObject* parent);

  MinMaxFloatOutlet(DataStream::Deserializer& vis, QObject* parent);
  MinMaxFloatOutlet(JSONObject::Deserializer& vis, QObject* parent);
  MinMaxFloatOutlet(DataStream::Deserializer&& vis, QObject* parent);
  MinMaxFloatOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  void forChildInlets(const smallfun::function<void (Inlet&)>& f) const noexcept override
  {
    f(*minInlet);
    f(*maxInlet);
  }

  void mapExecution(ossia::outlet& e, const smallfun::function<void (Inlet&, ossia::inlet&)>& f) const noexcept override
  {
    auto exec = safe_cast<ossia::minmax_float_outlet*>(&e);
    f(*minInlet, exec->min_inlet);
    f(*maxInlet, exec->max_inlet);
  }

  VIRTUAL_CONSTEXPR PortType type() const noexcept override { return Process::PortType::Message; }

  std::unique_ptr<Process::FloatSlider> minInlet;
  std::unique_ptr<Process::FloatSlider> maxInlet;
};
}

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::MinMaxFloatOutlet,
    "047e4cc2-4d99-4e8b-bf98-206018d02274")
W_OBJECT_IMPL(Process::MinMaxFloatOutlet)
namespace Process
{
MODEL_METADATA_IMPL_CPP(MinMaxFloatOutlet)
MinMaxFloatOutlet::MinMaxFloatOutlet(Id<Process::Port> c, QObject* parent)
    : ValueOutlet{std::move(c), parent}
    , minInlet{std::make_unique<FloatSlider>(Id<Process::Port>{0}, this)}
    , maxInlet{std::make_unique<FloatSlider>(Id<Process::Port>{1}, this)}
{
}
MinMaxFloatOutlet::~MinMaxFloatOutlet() {}

MinMaxFloatOutlet::MinMaxFloatOutlet(DataStream::Deserializer& vis, QObject* parent)
    : ValueOutlet{vis, parent}
{
  vis.writeTo(*this);
}
MinMaxFloatOutlet::MinMaxFloatOutlet(JSONObject::Deserializer& vis, QObject* parent)
    : ValueOutlet{vis, parent}
{
  vis.writeTo(*this);
}
MinMaxFloatOutlet::MinMaxFloatOutlet(DataStream::Deserializer&& vis, QObject* parent)
    : ValueOutlet{vis, parent}
{
  vis.writeTo(*this);
}
MinMaxFloatOutlet::MinMaxFloatOutlet(JSONObject::Deserializer&& vis, QObject* parent)
    : ValueOutlet{vis, parent}
{
  vis.writeTo(*this);
}

}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::MinMaxFloatOutlet>(const Process::MinMaxFloatOutlet& p)
{
  // read((Process::Inlet&)p);
  m_stream
      << *p.minInlet << *p.maxInlet;
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::MinMaxFloatOutlet>(Process::MinMaxFloatOutlet& p)
{
  static auto& il = components.interfaces<Process::PortFactoryList>();

  p.minInlet.reset((Process::FloatSlider*)deserialize_interface(il, *this, &p));
  p.maxInlet.reset((Process::FloatSlider*)deserialize_interface(il, *this, &p));
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::MinMaxFloatOutlet>(const Process::MinMaxFloatOutlet& p)
{
  // read((Process::Inlet&)p);
  obj["MinInlet"] = toJsonObject(*p.minInlet);
  obj["MaxInlet"] = toJsonObject(*p.maxInlet);
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::MinMaxFloatOutlet>(Process::MinMaxFloatOutlet& p)
{
  static auto& il = components.interfaces<Process::PortFactoryList>();

  {
    JSONObjectWriter writer{obj["MinInlet"].toObject()};
    p.minInlet.reset((Process::FloatSlider*)deserialize_interface(il, writer, &p));
  }
  {
    JSONObjectWriter writer{obj["MaxInlet"].toObject()};
    p.maxInlet.reset((Process::FloatSlider*)deserialize_interface(il, writer, &p));
  }
}

#include <wobjectimpl.h>
W_OBJECT_IMPL(Automation::ProcessModel)

namespace Automation
{

void ProcessModel::init()
{
  outlet->setCustomData("Out");
  m_outlets.push_back(outlet.get());
  connect(
      outlet.get(),
      &Process::Port::addressChanged,
      this,
      [=](const State::AddressAccessor& arg) {
        addressChanged(arg);
        prettyNameChanged();
        unitChanged(arg.qualifiers.get().unit);
        m_curve->changed();
      });
}

ProcessModel::ProcessModel(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : CurveProcessModel{duration,
                        id,
                        Metadata<ObjectKey_k, ProcessModel>::get(),
                        parent}
    , outlet{std::make_unique<Process::MinMaxFloatOutlet>(Id<Process::Port>(0), this)}
    , m_min{0.}
    , m_max{1.}
    , m_startState{new ProcessState{*this, 0., this}}
    , m_endState{new ProcessState{*this, 1., this}}
{
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

ProcessModel::~ProcessModel() {}

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

QString ProcessModel::prettyName() const noexcept
{
  auto res = address().toString_unsafe();
  if (!res.isEmpty())
    return res;

  // TODO we could use the customData of the port the automation is connected
  // to if any
  return "Automation";
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

void ProcessModel::setCurve_impl()
{
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
    prettyNameChanged();
    unitChanged(u);
  }
}
}
