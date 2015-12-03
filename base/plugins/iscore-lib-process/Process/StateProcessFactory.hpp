#pragma once
#include <Process/StateProcessFactoryKey.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <QByteArray>
#include <QString>

class StateProcess;

class StateProcessFactory :
        public iscore::GenericFactoryInterface<StateProcessFactoryKey>
{
        ISCORE_FACTORY_DECL("StateProcess")

    public:
            using factory_key_type = StateProcessFactoryKey;
        virtual ~StateProcessFactory();
        virtual QString prettyName() const = 0;

        virtual StateProcess* make(
                const Id<StateProcess>& id,
                QObject* parent) = 0;

        // throws if the serialization method is not implemented by the subclass
        virtual StateProcess* load(
                const VisitorVariant&,
                QObject* parent) = 0;
};
