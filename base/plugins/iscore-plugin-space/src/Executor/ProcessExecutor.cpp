#include "ProcessExecutor.hpp"
#include <src/SpaceProcess.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <src/LocalTree/AreaComponent.hpp>
#include <src/LocalTree/ComputationComponent.hpp>

#include <Editor/Message.h>
namespace Space
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        ProcessModel& process,
        DeviceDocumentPlugin& devices):
    m_process{process},
    m_devices{devices.list()}
{

}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue&,
        const OSSIA::TimeValue&)
{
    using namespace GiNaC;
    auto& devlist = m_process.context().devices.list();

    // For each area whose parameters depend on an address,
    // get the current area value and update it.
    for(AreaModel& area : m_process.areas)
    {
        const auto& parameter_map = area.parameterMapping();
        ValMap mapping;

        for(const auto& elt : parameter_map)
        {
            // We always set the default value just in case.
            auto it_pair = mapping.insert(
                               std::make_pair(
                                   ex_to<symbol>(elt.first).get_name(),
                                   State::convert::value<double>(elt.second.value)
                                   )
                               );

            auto& addr = elt.second.address;

            if(!addr.device.isEmpty())
            {
                // We fetch it from the device tree
                auto dev_it = devlist.find(addr.device);
                if(dev_it != devlist.devices().end())
                {
                    auto val = (*dev_it)->refresh(addr);
                    if(val)
                    {
                       it_pair.first->second = State::convert::value<double>(*val);
                    }
                }
            }
        }

        area.setCurrentMapping(mapping);
    }


    auto state = OSSIA::State::create();
    // State of each area
    // Shall be done in the "tree" component.
    /*
    for(const AreaModel& area : m_process.areas())
    {
        makeState(*state, area);
    }
    */


    // Shall be done either here, or in the tree component : choose between reactive, and state mode.
    // Same problem for "mapping" plug-in : react to changes or return state ?
    // "Reactive" execution component (has to be enabled / disabled on start / end)
    // vs "state" execution component
    // Handle computations / collisions
    for(const ComputationModel& computation : m_process.computations)
    {
        // We look for its tree component
        auto compo_it = find_if(
                            computation.components,
                            [] (iscore::Component& comp)
        { return dynamic_cast<LocalTree::ComputationComponent*>(&comp); });

        if(compo_it != computation.components.end())
        {
            auto& compo = static_cast<LocalTree::ComputationComponent&>(*compo_it);
            ISCORE_ASSERT(compo.valueNode()->getAddress().get());
            auto mess = OSSIA::Message::create(
                            compo.valueNode()->getAddress(),
                            new OSSIA::Float(computation.computation()()));
            state->stateElements().push_back(std::move(mess));
        }

    }

    // Send the parameters of each area
    // (variables's value ? default computations (like diameter, etc. ?))
    // For each computation, send the new state.

    return state;
}
}
}
