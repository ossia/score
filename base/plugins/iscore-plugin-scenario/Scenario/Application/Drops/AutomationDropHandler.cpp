#include "AutomationDropHandler.hpp"
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Scenario
{

bool AutomationDropHandler::handle(
        const ConstraintModel& cst,
        const QMimeData* mime)
{
    // TODO refactor with AddressEditWidget
    if(!mime->formats().contains(iscore::mime::nodelist()))
        return false;
    // TODO c.f. DeviceExplorerModel:784
/*
    Mime<Device::NodeList>::Deserializer des{*mime};
    Device::NodeList nl = des.deserialize();
    if(nl.empty())
        return false;

    std::vector<Device::FullAddressSettings> addresses;
    for(auto& node: nl)
    {
        // TODO refactor with CreateCurves
        if(node->is<Device::AddressSettings>())
        {
            const Device::AddressSettings& addr = node->get<Device::AddressSettings>();
            if(addr.value.val.isNumeric())
            {
                Device::FullAddressSettings as;
                static_cast<Device::AddressSettingsCommon&>(as) = addr;
                as.address = Device::address(*node);
                addresses.push_back(std::move(as));
            }
        }
    }

    if(addresses.empty())
        return false;

    auto& doc = iscore::IDocument::documentContext(cst);
    CreateCurvesFromAddresses({&cst}, addresses, doc.commandStack);
    */
    return true;
}

}
