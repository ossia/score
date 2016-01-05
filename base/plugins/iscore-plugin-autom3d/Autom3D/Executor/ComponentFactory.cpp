#include "ComponentFactory.hpp"
#include "Component.hpp"
#include <Autom3D/Autom3DModel.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
namespace Autom3D
{
namespace Executor
{

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
    static factory_key_type k("Autom3DComponent");
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
