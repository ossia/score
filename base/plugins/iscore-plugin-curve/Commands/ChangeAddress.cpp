#include "ChangeAddress.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include "Plugin/Panel/DeviceExplorerModel.hpp"
#include "Automation/AutomationModel.hpp"
ChangeAddress::ChangeAddress(ObjectPath &&path, const iscore::Address &newval):
    iscore::SerializableCommand{
        "AutomationControl", commandName(), description()},
    m_path{path}
{
    auto& autom = m_path.find<AutomationModel>();
    auto deviceexplorer = iscore::IDocument::documentFromObject(autom)
                          ->findChild<DeviceExplorerModel*>("DeviceExplorerModel");

    // Note : since we change the address, we also have to update the min / max if possible.
    // To do this, we must go and check into the device explorer.
    // If the node isn't found, we fallback on common values.

    // Get the current data.
    auto oldpath = autom.address().path;
    oldpath.prepend(autom.address().device);
    auto old_n = iscore::try_getNodeFromString(&deviceexplorer->rootNode(), std::move(oldpath));
    if(old_n)
    {
        Q_ASSERT(!old_n->isDevice());
        m_old = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(old_n->addressSettings(), autom.address());
    }
    else
    {
        m_old.address = autom.address();
        m_old.domain.min.val = autom.min();
        m_old.domain.max.val = autom.max();
    }

    // Get the new data.
    auto newpath = newval.path;
    newpath.prepend(newval.device);
    auto new_n = iscore::try_getNodeFromString(&deviceexplorer->rootNode(), std::move(newpath));
    if(new_n)
    {
        Q_ASSERT(!new_n->isDevice());
        m_new = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_child>(new_n->addressSettings(), newval);
    }
    else
    {
        m_new.address = newval;
        m_new.domain.min.val = 0.;
        m_new.domain.max.val = 1.;
    }
}


void ChangeAddress::undo()
{
    auto& autom = m_path.find<AutomationModel>();

    autom.setMin(m_old.domain.min.val.toDouble());
    autom.setMax(m_old.domain.max.val.toDouble());

    autom.setAddress(m_old.address);
}

void ChangeAddress::redo()
{
    auto& autom = m_path.find<AutomationModel>();

    autom.setMin(m_new.domain.min.val.toDouble());
    autom.setMax(m_new.domain.max.val.toDouble());

    autom.setAddress(m_new.address);
}

void ChangeAddress::serializeImpl(QDataStream & s) const
{
    s << m_path << m_old << m_new;
}

void ChangeAddress::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_old >> m_new;
}
