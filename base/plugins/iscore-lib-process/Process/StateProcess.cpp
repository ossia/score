#include "StateProcess.hpp"
namespace Process
{
StateProcess::StateProcess(
        const Id<StateProcess>& id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<StateProcess>{id, name, parent}
{

}

ISCORE_LIB_PROCESS_EXPORT StateProcess::~StateProcess() = default;

}
