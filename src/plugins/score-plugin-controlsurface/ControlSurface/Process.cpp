#include "Process.hpp"

#include <Curve/CurveModel.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(ControlSurface::Model)
namespace ControlSurface
{

Model::Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "ControlSurfaceProcess", parent}
{
  metadata().setInstanceName(*this);
}

Model::~Model() { }

// template<typename T>
// struct SliderWithOutputAddress final : public T
// {
//   static auto static_concreteKey() noexcept
//   {
//     return Metadata<ConcreteKey_k, SliderWithOutputAddress<T>>::get();
//   }
//   auto concreteKey() const noexcept override
//   {
//     return static_concreteKey();
//   }
//   void serialize_impl(const VisitorVariant& vis) const noexcept override
//   {
//     score::serialize_dyn(vis, *this);
//   }
//
//   W_OBJECT(SliderWithOutputAddress<T>)
//   using base_type = Process::ControlInlet;
//   using control_type = WidgetFactory::IntSlider;
//   template<typename... Args>
//   SliderWithOutputAddress(Args&&... args)
//     : T{std::forward<Args>(args)...}
//   {
//   }
//
//   void setOutputAddress(const State::AddressAccessor& addr)
//   {
//     if(addr != m_outputAddr)
//     {
//       m_outputAddr = addr;
//       outputAddressChanged(m_outputAddr);
//     }
//   }
//
//   const State::AddressAccessor& outputAddress() const noexcept
//   {
//     return m_outputAddr;
//   }
//
//   void outputAddressChanged(const State::AddressAccessor& addr)
//   W_SIGNAL(outputAddressChanged, addr)
//
// private:
//   State::AddressAccessor m_outputAddr;
// };
//
// W_OBJECT_IMPL(SliderWithOutputAddress<T>, template<typename T>)
// UUID_METADATA(, WidgetInletFactory<SliderWithOutputAddress<IntSlider>>,
// SliderWithOutputAddress<Process::IntSlider>,
// "a8e60bde-5e81-4b37-ba58-a9047da6a850");
Process::ControlInlet* makeControlFromType(
    const Id<Process::Port>& id,
    const Device::FullAddressAccessorSettings& addr,
    QObject* parent)
{
  // SliderWithOutputAddress<Process::IntSlider>();
  // TODO make better widgets if we have more information.

  auto& unit = addr.address.qualifiers.get().unit.v;
  if(unit.target<ossia::color_u>())
  {
    return new Process::HSVSlider{id, parent};
  }
  if(unit.target<ossia::position_u>() && addr.value.get_type() == ossia::val_type::VEC2F)
  {
    return new Process::XYSlider{id, parent};
  }
  switch (addr.value.get_type())
  {
    case ossia::val_type::IMPULSE:
      return new Process::Button{id, parent};
    case ossia::val_type::INT:
      return new Process::IntSlider{id, parent};
    case ossia::val_type::FLOAT:
      return new Process::FloatSlider{id, parent};
    case ossia::val_type::BOOL:
      return new Process::Toggle{id, parent};
    case ossia::val_type::STRING:
      return new Process::LineEdit{id, parent};
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return new Process::MultiSlider{id, parent};
    default:
      return new Process::ControlInlet(id, parent);
  }
}

void Model::setupControl(Process::ControlInlet* ctl, const State::AddressAccessor& addr)
{
  m_outputAddresses[ctl->id()] = addr;
  ctl->hidden = true;

  inlets().push_back(ctl);
  controlAdded(ctl->id());
}

void Model::addControl(const Id<Process::Port>& id, const Device::FullAddressAccessorSettings& msg)
{
  auto ctl = makeControlFromType(id, msg, this);
  // ctl->setAddress(msg.address);
  ctl->setValue(msg.value);
  ctl->setDomain(msg.domain);
  if (auto desc = ossia::net::get_description(msg.extendedAttributes))
  {
    ctl->setDescription(QString::fromStdString(*desc));
  }

  auto setName = [ctl](const State::AddressAccessor& addr) {
    int length_limit = 20;
    if (addr.address.path.isEmpty())
    {
      ctl->setCustomData("-");
    }
    else
    {
      auto quals = State::toString(addr.qualifiers);
      length_limit -= quals.length();

      QString str;

      auto it = addr.address.path.rbegin();
      while (it != addr.address.path.rend())
      {
        QString new_str = str;

        new_str.prepend("/" + *it);

        ++it;
        if (new_str.length() <= length_limit)
        {
          str = new_str;
        }
        else
        {
          break;
        }
      }

      // Remove the first / if it's not a whole address
      if (it != addr.address.path.rend())
        str.remove(0, 1);

      ctl->setCustomData(str + quals);
    }
  };

  setName(msg.address);

  setupControl(ctl, msg.address);

  //  QObject::connect(ctl, &Process::ControlInlet::addressChanged,
  //                   ctl, setName);
}

void Model::removeControl(const Id<Process::Port>& m_id)
{
  m_outputAddresses.erase(m_id);

  auto it = ossia::find_if(inlets(), [&](const auto& inlet) { return inlet->id() == m_id; });
  SCORE_ASSERT(it != inlets().end());
  controlRemoved(**it);
  auto ptr = *it;
  inlets().erase(it);
  delete ptr;
}

QString Model::prettyName() const noexcept
{
  return tr("Control surface");
}

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept { }

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept { }

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept { }
}
template <>
void DataStreamReader::read(const ControlSurface::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  m_stream << proc.m_outputAddresses;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ControlSurface::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
  m_stream >> proc.m_outputAddresses;
  checkDelimiter();
}

template <>
void JSONReader::read(const ControlSurface::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  obj["Addresses"] = proc.m_outputAddresses;
}

template <>
void JSONWriter::write(ControlSurface::Model& proc)
{
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);

  proc.m_outputAddresses <<= obj["Addresses"];
}
