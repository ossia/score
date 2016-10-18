#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Mapping/MappingModel.hpp>
#include <QString>
#include <QStringList>
#include <algorithm>

#include "ChangeAddresses.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Mapping
{
ChangeSourceAddress::ChangeSourceAddress(
        const ProcessModel& mapping,
        const State::AddressAccessor &newval):
    m_path{mapping},
    m_old{mapping.sourceAddress(),
          mapping.sourceMin(),
          mapping.sourceMax()},
    m_new{Explorer::makeFullAddressAccessorSettings(
              newval,
              iscore::IDocument::documentContext(mapping), 0., 1.)}
{
}

void ChangeSourceAddress::undo() const
{
    auto& mapping = m_path.find();

    mapping.setSourceMin(State::convert::value<double>(m_old.domain.min));
    mapping.setSourceMax(State::convert::value<double>(m_old.domain.max));

    mapping.setSourceAddress(m_old.address);
}

void ChangeSourceAddress::redo() const
{
    auto& mapping = m_path.find();

    mapping.setSourceMin(State::convert::value<double>(m_new.domain.min));
    mapping.setSourceMax(State::convert::value<double>(m_new.domain.max));

    mapping.setSourceAddress(m_new.address);
}

void ChangeSourceAddress::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeSourceAddress::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_old >> m_new;
}



ChangeTargetAddress::ChangeTargetAddress(
        const ProcessModel& mapping,
        const State::AddressAccessor &newval):
    m_path{mapping},
    m_old{mapping.targetAddress(),
          mapping.targetMin(),
          mapping.targetMax()},
    m_new{Explorer::makeFullAddressAccessorSettings(
              newval,
              iscore::IDocument::documentContext(mapping), 0., 1.)}
{
}


void ChangeTargetAddress::undo() const
{
    auto& mapping = m_path.find();

    mapping.setTargetMin(State::convert::value<double>(m_old.domain.min));
    mapping.setTargetMax(State::convert::value<double>(m_old.domain.max));

    mapping.setTargetAddress(m_old.address);
}

void ChangeTargetAddress::redo() const
{
    auto& mapping = m_path.find();

    mapping.setTargetMin(State::convert::value<double>(m_new.domain.min));
    mapping.setTargetMax(State::convert::value<double>(m_new.domain.max));

    mapping.setTargetAddress(m_new.address);
}

void ChangeTargetAddress::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeTargetAddress::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_old >> m_new;
}
}
