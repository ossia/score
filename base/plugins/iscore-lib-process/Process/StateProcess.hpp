#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/IdentifiedObject.hpp>

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

  ISCORE_SERIALIZE_FRIENDS

public:
  StateProcess(
      const Id<StateProcess>& id, const QString& name, QObject* parent);

  StateProcess(DataStream::Deserializer& vis, QObject* parent);
  StateProcess(JSONObject::Deserializer& vis, QObject* parent);

  virtual ~StateProcess();

  virtual StateProcess*
  clone(const Id<StateProcess>& newId, QObject* newParent) const = 0;

  // A user-friendly text to show to the users
  virtual QString prettyName() const = 0;
  virtual QString prettyShortName() const = 0;
};
}

DEFAULT_MODEL_METADATA(Process::StateProcess, "State process")
