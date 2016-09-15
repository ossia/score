#pragma once
#include <iscore/tools/Metadata.hpp>
#include <QObject>

namespace Process
{
class ProcessModelFactory; class LayerFactory;
class StateProcessFactory;
}


#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
    MODEL_METADATA(Export, Process::ProcessModelFactory, Model, Uuid, ObjectKey, PrettyName)

#define LAYER_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
    MODEL_METADATA(Export, Process::LayerFactory, Model, Uuid, ObjectKey, PrettyName)

#define STATEPROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
    MODEL_METADATA(Export, Process::StateProcessFactory, Model, Uuid, ObjectKey, PrettyName)
