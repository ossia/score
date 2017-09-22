#pragma once
#include <QObject>
#include <score/tools/Metadata.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>

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

#define PROCESS_METADATA_IMPL(Model) \
  MODEL_METADATA_IMPL(Model) \
  QString prettyShortName() const override { return Metadata<PrettyName_k, Model>::get(); }
