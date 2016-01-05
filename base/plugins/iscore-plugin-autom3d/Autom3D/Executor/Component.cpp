#include "Component.hpp"
#include "Executor.hpp"
#include <Autom3D/Autom3DModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
namespace Autom3D
{
namespace Executor
{

ProcessComponent::ProcessComponent(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        ProcessModel& element,
        const RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    RecreateOnPlay::ProcessComponent{parentConstraint, element, id, "Autom3DComponent", parent}
{
    auto proc = std::make_shared<ProcessExecutor>(
                element.address(),
                element.handles(),
                ctx.devices.list());
    m_ossia_process = proc;
}

const iscore::Component::Key& ProcessComponent::key() const
{
    static iscore::Component::Key k("Autom3DComponent");
    return k;
}
}
}
