// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ChangeAddresses.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Mapping/MappingModel.hpp>
#include <State/Address.hpp>
#include <State/Domain.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

namespace Mapping
{
ChangeSourceAddress::ChangeSourceAddress(
    const ProcessModel& mapping,
    Device::FullAddressAccessorSettings newval)
    : m_path{mapping}
    , m_old{mapping.sourceAddress(), mapping.sourceMin(), mapping.sourceMax()}
    , m_new{std::move(newval)}
{
  Curve::CurveDomain c(m_new.domain.get(), m_new.value);
  m_new.domain.get() = ossia::make_domain(c.min, c.max);
}

void ChangeSourceAddress::undo(const score::DocumentContext& ctx) const
{
  auto& mapping = m_path.find(ctx);

  auto& dom = m_old.domain.get();
  mapping.setSourceMin(dom.convert_min<double>());
  mapping.setSourceMax(dom.convert_max<double>());

  mapping.setSourceAddress(m_old.address);
}

void ChangeSourceAddress::redo(const score::DocumentContext& ctx) const
{
  auto& mapping = m_path.find(ctx);

  auto& dom = m_new.domain.get();
  mapping.setSourceMin(dom.convert_min<double>());
  mapping.setSourceMax(dom.convert_max<double>());

  mapping.setSourceAddress(m_new.address);
}

void ChangeSourceAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void ChangeSourceAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}

ChangeTargetAddress::ChangeTargetAddress(
    const ProcessModel& mapping,
    Device::FullAddressAccessorSettings newval)
    : m_path{mapping}
    , m_old{mapping.targetAddress(), mapping.targetMin(), mapping.targetMax()}
    , m_new{std::move(newval)}
{
  Curve::CurveDomain c(m_new.domain.get(), m_new.value);
  m_new.domain.get() = ossia::make_domain(c.min, c.max);
}

void ChangeTargetAddress::undo(const score::DocumentContext& ctx) const
{
  auto& mapping = m_path.find(ctx);

  auto& dom = m_old.domain.get();
  mapping.setTargetMin(dom.convert_min<double>());
  mapping.setTargetMax(dom.convert_max<double>());

  mapping.setTargetAddress(m_old.address);
}

void ChangeTargetAddress::redo(const score::DocumentContext& ctx) const
{
  auto& mapping = m_path.find(ctx);

  auto& dom = m_new.domain.get();
  mapping.setTargetMin(dom.convert_min<double>());
  mapping.setTargetMax(dom.convert_max<double>());

  mapping.setTargetAddress(m_new.address);
}

void ChangeTargetAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void ChangeTargetAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}
}
