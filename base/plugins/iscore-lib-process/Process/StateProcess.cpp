#include "StateProcess.hpp"
namespace Process
{
StateProcess::StateProcess(
    const Id<StateProcess>& id, const QString& name, QObject* parent)
    : IdentifiedObject<StateProcess>{id, name, parent}
{
}

StateProcess::StateProcess(Deserializer<JSONObject>& vis, QObject* parent)
    : IdentifiedObject(vis, parent)
{
  vis.writeTo(*this);
}

StateProcess::StateProcess(Deserializer<DataStream>& vis, QObject* parent)
    : IdentifiedObject(vis, parent)
{
  vis.writeTo(*this);
}

ISCORE_LIB_PROCESS_EXPORT StateProcess::~StateProcess() = default;
}
