#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <QString>
#include <QStringList>
#include <algorithm>

#include <Automation/AutomationModel.hpp>
#include "ChangeAddress.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Automation
{
ChangeAddress::ChangeAddress(
        Path<ProcessModel> &&path,
        const ::State::AddressAccessor &newval):
    m_path{std::move(path)},
    m_newAddress{newval}
{
    auto& autom = m_path.find();

    // Get the current data.
    m_oldAddress = autom.address();
    m_oldDomain.min.val = autom.min();
    m_oldDomain.max.val = autom.max();

    if(auto deviceexplorer = Explorer::try_deviceExplorerFromObject(autom))
    {
        // Note : since we change the address, we also have to update the min / max if possible.
        // To do this, we must go and check into the device explorer.
        // If the node isn't found, we fallback on common values.

        // Get the new data.
        auto newpath = newval.address.path;
        newpath.prepend(newval.address.device);
        auto new_n = Device::try_getNodeFromString(deviceexplorer->rootNode(), std::move(newpath));
        if(new_n)
        {
            ISCORE_ASSERT(new_n->is<Device::AddressSettings>());
            auto& addr = new_n->get<Device::AddressSettings>();

            m_newDomain = addr.domain;
            if(!newval.qualifiers.unit)
              m_newAddress.qualifiers.unit = addr.unit;
        }
        else
        {
            m_newAddress = newval;
            m_newDomain.min.val = 0.;
            m_newDomain.max.val = 1.;
        }
    }
}

ChangeAddress::ChangeAddress(
        Path<ProcessModel> &&path,
        Device::FullAddressSettings newval):
    m_path{path},
    m_newAddress{newval.address},
    m_newDomain{newval.domain}
{
    m_newAddress.qualifiers.unit = newval.unit;

    auto& autom = m_path.find();
    m_oldAddress = autom.address();
    m_oldDomain.min.val = autom.min();
    m_oldDomain.max.val = autom.max();
}


void ChangeAddress::undo() const
{
    auto& autom = m_path.find();

    {
        //QSignalBlocker blck{autom.curve()};
        autom.setMin(::State::convert::value<double>(m_oldDomain.min));
        autom.setMax(::State::convert::value<double>(m_oldDomain.max));
        autom.setAddress(m_oldAddress);
    }
    // autom.curve().changed();

}

void ChangeAddress::redo() const
{
    auto& autom = m_path.find();

    {
        //QSignalBlocker blck{autom.curve()};
        autom.setMin(::State::convert::value<double>(m_newDomain.min));
        autom.setMax(::State::convert::value<double>(m_newDomain.max));
        autom.setAddress(m_newAddress);
    }
    // autom.curve().changed();
}

void ChangeAddress::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_oldAddress << m_newAddress << m_oldDomain << m_newDomain;
}

void ChangeAddress::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_oldAddress >> m_newAddress >> m_oldDomain >> m_newDomain;
}
}
