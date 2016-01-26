#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>

namespace Process { class ProcessModel; }
using ProcessFactoryKey = UuidKey<Process::ProcessModel>;
Q_DECLARE_METATYPE(ProcessFactoryKey)
