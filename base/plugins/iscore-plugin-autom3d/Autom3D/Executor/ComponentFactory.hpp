#pragma once

#include <OSSIA/Executor/ProcessElement.hpp>
namespace Autom3D
{
namespace Executor
{
class ProcessComponentFactory final :
        public RecreateOnPlay::ProcessComponentFactory
{
    public:
        virtual ~ProcessComponentFactory();

        virtual RecreateOnPlay::ProcessComponent* make(
                RecreateOnPlay::ConstraintElement& cst,
                Process::ProcessModel& proc,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel& proc,
                const RecreateOnPlay::DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
}
