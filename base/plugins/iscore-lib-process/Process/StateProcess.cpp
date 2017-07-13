// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateProcess.hpp"
namespace Process
{
StateProcess::StateProcess(
    const Id<StateProcess>& id, const QString& name, QObject* parent)
    : IdentifiedObject<StateProcess>{id, name, parent}
{
}

StateProcess::StateProcess(JSONObject::Deserializer& vis, QObject* parent)
    : IdentifiedObject(vis, parent)
{
  vis.writeTo(*this);
}

StateProcess::StateProcess(DataStream::Deserializer& vis, QObject* parent)
    : IdentifiedObject(vis, parent)
{
  vis.writeTo(*this);
}

ISCORE_LIB_PROCESS_EXPORT StateProcess::~StateProcess() = default;
}
