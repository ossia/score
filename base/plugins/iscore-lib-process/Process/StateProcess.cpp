#include "StateProcess.hpp"

StateProcess::StateProcess(
        const Id<StateProcess>& id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<StateProcess>{id, name, parent}
{

}

StateProcess::~StateProcess()
{

}
