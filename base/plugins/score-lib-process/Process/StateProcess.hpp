#pragma once
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/model/IdentifiedObject.hpp>

#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/tools/Metadata.hpp>
#include <score_lib_process_export.h>

namespace Process
{
class StateProcessFactory;
class SCORE_LIB_PROCESS_EXPORT StateProcess
    : public IdentifiedObject<StateProcess>,
      public score::SerializableInterface<StateProcessFactory>
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS

public:
  StateProcess(
      const Id<StateProcess>& id, const QString& name, QObject* parent);

  StateProcess(DataStream::Deserializer& vis, QObject* parent);
  StateProcess(JSONObject::Deserializer& vis, QObject* parent);

  virtual ~StateProcess();

  // A user-friendly text to show to the users
  virtual QString prettyName() const = 0;
  virtual QString prettyShortName() const = 0;
  virtual QString category() const = 0;
  virtual QStringList tags() const = 0;
};
}

DEFAULT_MODEL_METADATA(Process::StateProcess, "State process")
