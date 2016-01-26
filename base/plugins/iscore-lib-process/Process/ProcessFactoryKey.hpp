#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>

class ProcessTag{};
using ProcessFactoryKey = UuidKey<ProcessTag>;
Q_DECLARE_METATYPE(ProcessFactoryKey)
