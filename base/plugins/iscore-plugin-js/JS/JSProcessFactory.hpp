#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Process/ProcessFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/StateProcess.hpp>
#include <JS/JSProcessMetadata.hpp>

#include <Process/StateProcessFactory.hpp>

namespace JS
{
using ProcessFactory = Process::GenericDefaultProcessFactory<JS::ProcessModel>;

// MOVEME
// REFACTORME
class StateProcessFactory : public Process::StateProcessFactory
{
    public:
        QString prettyName() const override
        { return Metadata<PrettyName_k, StateProcess>::get(); }

        const UuidKey<Process::StateProcessFactory>& concreteFactoryKey() const override
        { return Metadata<ConcreteFactoryKey_k, StateProcess>::get(); }

        StateProcess* make(const Id<Process::StateProcess>& id, QObject* parent) override
        { return new StateProcess{id, parent}; }

        Process::StateProcess* load(
                const VisitorVariant& vis,
                QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new StateProcess{deserializer, parent};});
        }
};

}
