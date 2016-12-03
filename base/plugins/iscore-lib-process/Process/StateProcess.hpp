#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_process_export.h>

namespace Process
{
class StateProcessFactory;
class ISCORE_LIB_PROCESS_EXPORT StateProcess
    : public IdentifiedObject<StateProcess>,
      public iscore::SerializableInterface<StateProcessFactory>
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS(StateProcess, DataStream)
  ISCORE_SERIALIZE_FRIENDS(StateProcess, JSONObject)

public:
  StateProcess(
      const Id<StateProcess>& id, const QString& name, QObject* parent);

  StateProcess(Deserializer<DataStream>& vis, QObject* parent);
  StateProcess(Deserializer<JSONObject>& vis, QObject* parent);

  virtual ~StateProcess();

  virtual StateProcess*
  clone(const Id<StateProcess>& newId, QObject* newParent) const = 0;

  // A user-friendly text to show to the users
  virtual QString prettyName() const = 0;
};
}

DEFAULT_MODEL_METADATA(Process::StateProcess, "State process")
