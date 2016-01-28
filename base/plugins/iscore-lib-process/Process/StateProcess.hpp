#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <Process/StateProcessFactoryKey.hpp>
#include <iscore_lib_process_export.h>
#include <iscore/tools/Metadata.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT StateProcess:
        public IdentifiedObject<StateProcess>,
        public iscore::SerializableInterface<StateProcess>
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(StateProcess, DataStream)
        ISCORE_SERIALIZE_FRIENDS(StateProcess, JSONObject)

    public:
        using IdentifiedObject<StateProcess>::IdentifiedObject;
        StateProcess(
                const Id<StateProcess>& id,
                const QString& name,
                QObject* parent);

        template<typename Impl>
        StateProcess(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual ~StateProcess();

        virtual StateProcess* clone(
                const Id<StateProcess>& newId,
                QObject* newParent) const = 0;

        // A user-friendly text to show to the users
        virtual QString prettyName() const = 0;
};
}

DEFAULT_MODEL_METADATA(Process::StateProcess, "State process")
