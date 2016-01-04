#pragma once
#include <Process/StateProcess.hpp>
#include <OSSIA/Executor/ProcessModel/ProcessModel.hpp>

#include <memory>

#include "Editor/StateElement.h"
#include "Editor/State.h"
#include "Editor/TimeValue.h"
#include <QDebug>
#include <QTime>

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

class SimpleStateProcessModel : public RecreateOnPlay::OSSIAStateProcessModel
{
    public:
        explicit SimpleStateProcessModel(
                const Id<StateProcess>& id,
                QObject* parent):
            RecreateOnPlay::OSSIAStateProcessModel{id, "toto", parent}
        {
            m_ossia_state = std::make_shared<SimpleStateProcess>();
        }

        explicit SimpleStateProcessModel(
                const SimpleStateProcessModel& source,
                const Id<StateProcess>& id,
                QObject* parent):
            RecreateOnPlay::OSSIAStateProcessModel{id, "toto", parent}
        {

        }

        template<typename Impl>
        explicit SimpleStateProcessModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            RecreateOnPlay::OSSIAStateProcessModel{vis, parent},
            m_ossia_state{std::make_shared<SimpleStateProcess>()}
        {
            vis.writeTo(*this);
        }

        SimpleStateProcessModel* clone(
                const Id<StateProcess>& newId,
                QObject* newParent) const override
        {
            return nullptr;
        }

        void serialize(const VisitorVariant& vis) const override
        {
        }

        const StateProcessFactoryKey&key() const override
        {
            static const StateProcessFactoryKey name{"toto"};
            return name;
        }

        QString prettyName() const override
        {
            return "toto";
        }

        std::shared_ptr<OSSIA::StateElement> state() const override
        {
            return m_ossia_state;
        }

    private:
        std::shared_ptr<OSSIA::StateElement> m_ossia_state;
};
