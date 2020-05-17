#include "MinMaxFloatPort.hpp"

#include <Process/Dataflow/PortFactory.hpp>

#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/SerializableInterface.hpp>

#include <ossia/dataflow/nodes/automation.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::MinMaxFloatOutlet)

namespace Process
{
MODEL_METADATA_IMPL_CPP(MinMaxFloatOutlet)
MinMaxFloatOutlet::MinMaxFloatOutlet(Id<Process::Port> c, QObject* parent)
    : ValueOutlet{std::move(c), parent}
    , minInlet{std::make_unique<FloatSlider>(Id<Process::Port>{0}, this)}
    , maxInlet{std::make_unique<FloatSlider>(Id<Process::Port>{1}, this)}
{
  minInlet->setValue(0.);
  maxInlet->setValue(1.);
}
MinMaxFloatOutlet::~MinMaxFloatOutlet() { }

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

void MinMaxFloatOutlet::forChildInlets(const smallfun::function<void(Inlet&)>& f) const noexcept
{
  /* TODO fix AutomationModel
  f(*minInlet);
  f(*maxInlet);
  */
}

void MinMaxFloatOutlet::mapExecution(
    ossia::outlet& e,
    const smallfun::function<void(Inlet&, ossia::inlet&)>& f) const noexcept
{
  /* TODO fix AutomationModel
  auto exec = safe_cast<ossia::minmax_float_outlet*>(&e);
  f(*minInlet, exec->min_inlet);
  f(*maxInlet, exec->max_inlet);
  */
}

}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::MinMaxFloatOutlet>(const Process::MinMaxFloatOutlet& p)
{
  // read((Process::Inlet&)p);
  m_stream << *p.minInlet << *p.maxInlet;
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
JSONReader::read<Process::MinMaxFloatOutlet>(const Process::MinMaxFloatOutlet& p)
{
  // read((Process::Inlet&)p);
  obj["MinInlet"] = *p.minInlet;
  obj["MaxInlet"] = *p.maxInlet;
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write<Process::MinMaxFloatOutlet>(Process::MinMaxFloatOutlet& p)
{
  static auto& il = components.interfaces<Process::PortFactoryList>();

  {
    JSONWriter writer{obj["MinInlet"]};
    p.minInlet.reset((Process::FloatSlider*)deserialize_interface(il, writer, &p));
  }
  {
    JSONWriter writer{obj["MaxInlet"]};
    p.maxInlet.reset((Process::FloatSlider*)deserialize_interface(il, writer, &p));
  }
}
