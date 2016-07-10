#pragma once
#include <iscore/tools/Metadata.hpp>
#include <QObject>

namespace Process
{
class ProcessFactory;
class StateProcessFactory;
}


#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
    MODEL_METADATA(Export, Process::ProcessFactory, Model, Uuid, ObjectKey, PrettyName)

#define STATEPROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
    MODEL_METADATA(Export, Process::StateProcessFactory, Model, Uuid, ObjectKey, PrettyName)
