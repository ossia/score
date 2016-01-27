#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>

namespace Process {
class StateProcess;
}
using StateProcessFactoryKey = UuidKey<Process::StateProcess>;
Q_DECLARE_METATYPE(StateProcessFactoryKey)
