#pragma once
#include <Process/StateProcess.hpp>

#include <memory>

#include "Editor/StateElement.h"
#include "Editor/State.h"
#include "Editor/TimeValue.h"
#include <QDebug>
#include <QTime>
namespace Process
{
class StateProcessFactory;
}

class SimpleStateProcess : public OSSIA::StateElement
{
    public:
        void launch() const override
        {
            qDebug() << QTime::currentTime();
        }

        OSSIA::StateElement::Type getType() const override
        {
            return OSSIA::StateElement::Type::USER;
        }
};

class SimpleStateProcessModel : public Process::StateProcess
{
    public:
        explicit SimpleStateProcessModel(
                const Id<StateProcess>& id,
                QObject* parent):
            Process::StateProcess{id, "toto", parent}
        {
        }

        explicit SimpleStateProcessModel(
                const SimpleStateProcessModel& source,
                const Id<StateProcess>& id,
                QObject* parent):
            Process::StateProcess{id, "toto", parent}
        {

        }

        template<typename Impl>
        explicit SimpleStateProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            Process::StateProcess{vis, parent}
        {
            vis.writeTo(*this);
        }

        SimpleStateProcessModel* clone(
                const Id<StateProcess>& newId,
                QObject* newParent) const override
        {
            return nullptr;
        }

        void serialize_impl(const VisitorVariant& vis) const override
        {
        }

        UuidKey<Process::StateProcessFactory> concreteFactoryKey() const override
        {
            static const UuidKey<Process::StateProcessFactory> name{"40517cca-3cbe-42bf-9bd4-982bc4696516"};
            return name;
        }

        QString prettyName() const override
        {
            return "toto";
        }

};
