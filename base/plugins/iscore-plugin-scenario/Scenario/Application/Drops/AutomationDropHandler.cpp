#include "AutomationDropHandler.hpp"
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Scenario
{

static void getAddressesRecursively(
        const Device::Node& node,
        State::Address curAddr,
        std::vector<Device::FullAddressSettings>& addresses)
{
    // TODO refactor with CreateCurves
    if(node.is<Device::AddressSettings>())
    {
        const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
        if(addr.value.val.isNumeric())
        {
            Device::FullAddressSettings as;
            static_cast<Device::AddressSettingsCommon&>(as) = addr;
            as.address = curAddr;
            addresses.push_back(std::move(as));
        }
    }

    for(auto& child : node)
    {
        const Device::AddressSettings& addr = child.get<Device::AddressSettings>();

        State::Address newAddr{curAddr};
        newAddr.path.append(addr.name);
        getAddressesRecursively(child, newAddr, addresses);
    }
}

bool AutomationDropHandler::handle(
        const ConstraintModel& cst,
        const QMimeData* mime)
{
    // TODO refactor with AddressEditWidget
    if(!mime->formats().contains(iscore::mime::nodelist()))
        return false;

    Mime<Device::FreeNodeList>::Deserializer des{*mime};
    Device::FreeNodeList nl = des.deserialize();
    if(nl.empty())
        return false;

    std::vector<Device::FullAddressSettings> addresses;
    for(auto& np: nl)
    {
        getAddressesRecursively(np.second, np.first, addresses);
    }

    if(addresses.empty())
        return false;

    auto& doc = iscore::IDocument::documentContext(cst);
    CreateCurvesFromAddresses({&cst}, addresses, doc.commandStack);

    return true;
}

}
