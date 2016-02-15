#pragma once
#include <Process/StateProcessFactory.hpp>
#include <SimpleProcess/SimpleStateProcess.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

class SimpleStateProcessFactory : public Process::StateProcessFactory
{


        // StateProcessFactory interface
    public:
        QString prettyName() const override
        { return QObject::tr("toto"); }

        const UuidKey<Process::StateProcessFactory>& concreteFactoryKey() const override
        {
            static const UuidKey<Process::StateProcessFactory>name{"40517cca-3cbe-42bf-9bd4-982bc4696516"};
            return name;
        }

        Process::StateProcess*make(const Id<Process::StateProcess>& id, QObject* parent) override
        {
            return new SimpleStateProcessModel(id, parent);
        }
        Process::StateProcess*load(const VisitorVariant& vis, QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new SimpleStateProcessModel{deserializer, parent};});
        }
};
