#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <QString>
#include <QStringList>
#include <algorithm>

#include "ChangeAddress.hpp"
#include <ossia/editor/value/value_conversion.hpp>
#include <Automation/AutomationModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>

#include <Automation/Color/GradientAutomModel.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>

namespace Automation
{
ChangeAddress::ChangeAddress(
    const ProcessModel& autom, const State::AddressAccessor& newval)
    : m_path{autom}
    , m_old{autom.address(), autom.min(), autom.max()}
    , m_new(Explorer::makeFullAddressAccessorSettings(
          newval, iscore::IDocument::documentContext(autom), 0., 1.))
{
  Curve::CurveDomain c(m_new.domain.get(), m_new.value);
  m_new.domain.get() = ossia::make_domain(c.min, c.max);
}

ChangeAddress::ChangeAddress(
    const ProcessModel& autom, Device::FullAddressAccessorSettings newval)
    : m_path{autom}
    , m_old{autom.address(), autom.min(), autom.max()}
    , m_new{std::move(newval)}
{
  Curve::CurveDomain c(m_new.domain.get(), m_new.value);
  m_new.domain.get() = ossia::make_domain(c.min, c.max);
}

ChangeAddress::ChangeAddress(
    const ProcessModel& autom, const Device::FullAddressSettings& newval)
    : m_path{autom}
{
  m_new.address = newval.address;
  m_new.domain = newval.domain;
  Curve::CurveDomain c(m_new.domain.get(), m_new.value);
  m_new.domain.get() = ossia::make_domain(c.min, c.max);
  m_new.address.qualifiers.get().unit = newval.unit;

  m_old.address = autom.address();
  m_old.domain = ossia::make_domain(autom.min(), autom.max());
}

void ChangeAddress::undo() const
{
  auto& autom = m_path.find();

  {
    // QSignalBlocker blck{autom.curve()};
    auto& dom = m_old.domain.get();
    autom.setMin(dom.convert_min<double>());
    autom.setMax(dom.convert_max<double>());

    autom.setAddress(m_old.address);
  }
  // autom.curve().changed();
}

void ChangeAddress::redo() const
{
  auto& autom = m_path.find();

  {
    // QSignalBlocker blck{autom.curve()};
    auto& dom = m_new.domain.get();
    autom.setMin(dom.convert_min<double>());
    autom.setMax(dom.convert_max<double>());
    autom.setAddress(m_new.address);
  }
  // autom.curve().changed();
}

void ChangeAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void ChangeAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}
}


namespace Gradient
{
ChangeGradientAddress::ChangeGradientAddress(
    const ProcessModel& autom, const State::AddressAccessor& newval)
    : m_path{autom}
    , m_old{autom.address()}
    , m_new{newval}
{
}

void ChangeGradientAddress::undo() const
{
  auto& autom = m_path.find();
  autom.setAddress(m_old);
}

void ChangeGradientAddress::redo() const
{
  auto& autom = m_path.find();
  autom.setAddress(m_new);
}

void ChangeGradientAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void ChangeGradientAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}
}

