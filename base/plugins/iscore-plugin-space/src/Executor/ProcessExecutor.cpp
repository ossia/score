#include "ProcessExecutor.hpp"
#include <src/SpaceProcess.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <src/LocalTree/AreaComponent.hpp>
#include <src/LocalTree/ComputationComponent.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <Editor/Message.h>
namespace Space
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        ProcessModel& process,
        const DeviceList& devices):
    m_process{process},
    m_devices{devices}
{

}

std::shared_ptr<OSSIA::StateElement> ProcessExecutor::state(
        const OSSIA::TimeValue& t,
        const OSSIA::TimeValue&)
{
    using namespace GiNaC;

    // For each area whose parameters depend on an address,
    // get the current area value and update it.
    for(AreaModel& area : m_process.areas)
    {
        // Handle time
        const auto& parameter_map = area.parameterMapping();
        ValMap mapping;

        for(const auto& elt : parameter_map.keys())
        {
            auto& val = parameter_map[elt];
            // We always set the default value just in case.
            auto it_pair = mapping.insert(
                               std::make_pair(
                                   elt.toStdString(),
                                   State::convert::value<double>(val.value)
                                   )
                               );

            auto& addr = val.address;
            if(!addr.device.isEmpty())
            {
                if(addr.device == "parent" && addr.path == QStringList{"t"})
                {
                    it_pair.first->second = 100 * t;
                }
                else
                {
                    // We fetch it from the device tree
                    auto dev_it = m_devices.find(addr.device);
                    if(dev_it != m_devices.devices().end())
                    {
                        auto val = (*dev_it)->refresh(addr);
                        if(val)
                        {
                           it_pair.first->second = State::convert::value<double>(*val);
                        }
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





ProcessComponent::ProcessComponent(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        Space::ProcessModel& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    RecreateOnPlay::ProcessComponent{parentConstraint, element, id, "SpaceComponent", parent}
{
    m_ossia_process = std::make_shared<ProcessExecutor>(element, ctx.devices.list());
}

const iscore::Component::Key&ProcessComponent::key() const
{
    static iscore::Component::Key k("SpaceComponent");
    return k;
}

ProcessComponentFactory::~ProcessComponentFactory()
{

}

RecreateOnPlay::ProcessComponent* ProcessComponentFactory::make(
        RecreateOnPlay::ConstraintElement& cst,
        Process::ProcessModel& proc,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent) const
{
    return new ProcessComponent{cst, static_cast<ProcessModel&>(proc), ctx, id, parent};
}

const ProcessComponentFactory::factory_key_type&
ProcessComponentFactory::key_impl() const
{
    static factory_key_type k("SpaceComponent");
    return k;
}

bool ProcessComponentFactory::matches(
        Process::ProcessModel& proc,
        const RecreateOnPlay::DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<ProcessModel*>(&proc);
}


}
}
