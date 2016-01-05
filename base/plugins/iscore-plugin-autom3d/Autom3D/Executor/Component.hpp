#pragma once

#include <OSSIA/Executor/ProcessElement.hpp>
namespace Autom3D
{
class ProcessModel;
namespace Executor
{

class ProcessComponent final : public RecreateOnPlay::ProcessComponent
{
    public:
        ProcessComponent(
                RecreateOnPlay::ConstraintElement& parentConstraint,
                ProcessModel& element,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        const Key &key() const override;
};
}
}
