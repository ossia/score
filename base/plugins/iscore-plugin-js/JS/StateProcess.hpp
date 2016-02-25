#pragma once
#include <Process/StateProcess.hpp>
#include <JS/JSProcessMetadata.hpp>
namespace JS
{
class StateProcess :
        public Process::StateProcess
{
        Q_OBJECT
        ISCORE_SERIALIZE_FRIENDS(JS::StateProcess, DataStream)
        ISCORE_SERIALIZE_FRIENDS(JS::StateProcess, JSONObject)
    public:
        explicit StateProcess(
                const Id<Process::StateProcess>& id,
                QObject* parent);

        explicit StateProcess(
                const StateProcess& source,
                const Id<Process::StateProcess>& id,
                QObject* parent):
            Process::StateProcess{id, Metadata<ObjectKey_k, StateProcess>::get(), parent},
            m_script{source.m_script}
        {

        }

        template<typename Impl>
        explicit StateProcess(
                Deserializer<Impl>& vis,
                QObject* parent) :
            Process::StateProcess{vis, parent}
        {
            vis.writeTo(*this);
        }


        void setScript(const QString& script);
        const QString& script() const
        { return m_script; }

        UuidKey<Process::StateProcessFactory> concreteFactoryKey() const override
        {
            return Metadata<ConcreteFactoryKey_k, StateProcess>::get();
        }
        void serialize_impl(const VisitorVariant& vis) const override;

    signals:
        void scriptChanged(QString);

    private:
        StateProcess* clone(
                const Id<Process::StateProcess>& newId,
                QObject* newParent) const override
        {
            return new StateProcess {*this, newId, newParent};
        }



        QString prettyName() const override
        {
            return Metadata<PrettyName_k, StateProcess>::get();
        }

        QString m_script;
};
}
