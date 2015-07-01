#pragma once
#include <QList>

namespace iscore
{
class SerializableCommand;
}

// This rollback only undoes creational commands as an optimization
struct ScenarioRollbackStrategy
{
    static void rollback(const QList<iscore::SerializableCommand*>& cmds);
};
