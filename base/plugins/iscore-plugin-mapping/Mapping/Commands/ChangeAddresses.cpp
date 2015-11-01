#include "ChangeAddresses.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Mapping/MappingModel.hpp>

// TODO try to template this to reuse it with ChangeAddress / ChangeTargetAddress
// TODO why not use AddressSettings directly on Automations / Mapping ? It would simplify...
ChangeSourceAddress::ChangeSourceAddress(
        Path<MappingModel> &&path,
        const iscore::Address &newval):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_path{path}
{
    auto& mapping = m_path.find();
    auto& deviceexplorer = deviceExplorerFromObject(mapping);

    // Note : since we change the address, we also have to update the min / max if possible.
    // To do this, we must go and check into the device explorer.
    // If the node isn't found, we fallback on common values.

    // Get the current data.
    m_old.address = mapping.sourceAddress();
    m_old.domain.min.val = mapping.sourceMin();
    m_old.domain.max.val = mapping.sourceMax();

    // Get the new data.
    auto newpath = newval.path;
    newpath.prepend(newval.device);
    auto new_n = iscore::try_getNodeFromString(deviceexplorer.rootNode(), std::move(newpath));
    if(new_n)
    {
        ISCORE_ASSERT(!new_n->is<iscore::DeviceSettings>());
        m_new = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(new_n->get<iscore::AddressSettings>(), newval);
    }
    else
    {
        m_new.address = newval;
        m_new.domain.min.val = 0.;
        m_new.domain.max.val = 1.;
    }
}


void ChangeSourceAddress::undo() const
{
    auto& mapping = m_path.find();

    mapping.setSourceMin(iscore::convert::value<double>(m_old.domain.min));
    mapping.setSourceMax(iscore::convert::value<double>(m_old.domain.max));

    mapping.setSourceAddress(m_old.address);
}

void ChangeSourceAddress::redo() const
{
    auto& mapping = m_path.find();

    mapping.setSourceMin(iscore::convert::value<double>(m_new.domain.min));
    mapping.setSourceMax(iscore::convert::value<double>(m_new.domain.max));

    mapping.setSourceAddress(m_new.address);
}

void ChangeSourceAddress::serializeImpl(QDataStream & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeSourceAddress::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_old >> m_new;
}






ChangeTargetAddress::ChangeTargetAddress(
        Path<MappingModel> &&path,
        const iscore::Address &newval):
    iscore::SerializableCommand{
        factoryName(), commandName(), description()},
    m_path{path}
{
    auto& mapping = m_path.find();
    auto& deviceexplorer = deviceExplorerFromObject(mapping);

    // Note : since we change the address, we also have to update the min / max if possible.
    // To do this, we must go and check into the device explorer.
    // If the node isn't found, we fallback on common values.

    // Get the current data.
    m_old.address = mapping.targetAddress();
    m_old.domain.min.val = mapping.targetMin();
    m_old.domain.max.val = mapping.targetMax();

    // Get the new data.
    auto newpath = newval.path;
    newpath.prepend(newval.device);
    auto new_n = iscore::try_getNodeFromString(deviceexplorer.rootNode(), std::move(newpath));
    if(new_n)
    {
        ISCORE_ASSERT(!new_n->is<iscore::DeviceSettings>());
        m_new = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(new_n->get<iscore::AddressSettings>(), newval);
    }
    else
    {
        m_new.address = newval;
        m_new.domain.min.val = 0.;
        m_new.domain.max.val = 1.;
    }
}


void ChangeTargetAddress::undo() const
{
    auto& mapping = m_path.find();

    mapping.setTargetMin(iscore::convert::value<double>(m_old.domain.min));
    mapping.setTargetMax(iscore::convert::value<double>(m_old.domain.max));

    mapping.setTargetAddress(m_old.address);
}

void ChangeTargetAddress::redo() const
{
    auto& mapping = m_path.find();

    mapping.setTargetMin(iscore::convert::value<double>(m_new.domain.min));
    mapping.setTargetMax(iscore::convert::value<double>(m_new.domain.max));

    mapping.setTargetAddress(m_new.address);
}

void ChangeTargetAddress::serializeImpl(QDataStream & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeTargetAddress::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_old >> m_new;
}
