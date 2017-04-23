#pragma once
#include <QObject>
#include <iscore/tools/Metadata.hpp>

namespace Process
{
class ProcessModel;
class ProcessModelFactory;
class LayerFactory;
class StateProcessFactory;
}

#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
  MODEL_METADATA(                                                    \
      Export, Process::ProcessModel, Model, Uuid, ObjectKey,  \
      PrettyName)

#define STATEPROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
  MODEL_METADATA(                                                         \
      Export, Process::StateProcessFactory, Model, Uuid, ObjectKey,       \
      PrettyName)
