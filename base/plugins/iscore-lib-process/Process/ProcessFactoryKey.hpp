#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

class ProcessTag{};
using ProcessFactoryKey = StringKey<ProcessTag>;
Q_DECLARE_METATYPE(ProcessFactoryKey)
