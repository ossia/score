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
        const ::State::Address &newval):
    m_path{path}
{
    auto& autom = m_path.find();

    // Get the current data.
    m_old.address = autom.address();
    m_old.domain.min.val = autom.min();
    m_old.domain.max.val = autom.max();

    if(auto deviceexplorer = Explorer::try_deviceExplorerFromObject(autom))
    {
        // Note : since we change the address, we also have to update the min / max if possible.
        // To do this, we must go and check into the device explorer.
        // If the node isn't found, we fallback on common values.

        // Get the new data.
        auto newpath = newval.path;
        newpath.prepend(newval.device);
        auto new_n = Device::try_getNodeFromString(deviceexplorer->rootNode(), std::move(newpath));
        if(new_n)
        {
            ISCORE_ASSERT(new_n->is<Device::AddressSettings>());
            m_new = Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(
                        new_n->get<Device::AddressSettings>(), newval);
        }
        else
        {
            m_new.address = newval;
            m_new.domain.min.val = 0.;
            m_new.domain.max.val = 1.;
        }
    }
}

ChangeAddress::ChangeAddress(
        Path<ProcessModel> &&path,
        Device::FullAddressSettings newval):
    m_path{path},
    m_new{std::move(newval)}
{
    auto& autom = m_path.find();
    m_old.address = autom.address();
    m_old.domain.min.val = autom.min();
    m_old.domain.max.val = autom.max();
}


void ChangeAddress::undo() const
{
    auto& autom = m_path.find();

    {
        QSignalBlocker blck{autom.curve()};
        autom.setMin(::State::convert::value<double>(m_old.domain.min));
        autom.setMax(::State::convert::value<double>(m_old.domain.max));

        autom.setAddress(m_old.address);
    }
    autom.curve().changed();
}

void ChangeAddress::redo() const
{
    auto& autom = m_path.find();

    {
        QSignalBlocker blck{autom.curve()};
        autom.setMin(::State::convert::value<double>(m_new.domain.min));
        autom.setMax(::State::convert::value<double>(m_new.domain.max));

        autom.setAddress(m_new.address);
    }
    autom.curve().changed();
}

void ChangeAddress::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeAddress::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_old >> m_new;
}
}
