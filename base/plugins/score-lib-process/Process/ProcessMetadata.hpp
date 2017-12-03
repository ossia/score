#pragma once
#include <QObject>
#include <QStringList>
#include <score/tools/Metadata.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>

namespace Process
{
class ProcessModel;
class ProcessModelFactory;
class LayerFactory;
class StateProcessFactory;
}

#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName, Category, Tags) \
  MODEL_METADATA(                                                    \
      Export, Process::ProcessModel, Model, Uuid, ObjectKey,         \
      PrettyName)                                                    \
  CATEGORY_METADATA(Export, Model, Category)                         \
  TAGS_METADATA(Export, Model, Tags)


#define STATEPROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName, Category, Tags) \
  MODEL_METADATA(                                                         \
      Export, Process::StateProcessFactory, Model, Uuid, ObjectKey,       \
      PrettyName)                                                         \
  CATEGORY_METADATA(Export, Model, Category)                              \
  TAGS_METADATA(Export, Model, Tags)

#define PROCESS_METADATA_IMPL(Model) \
  MODEL_METADATA_IMPL(Model) \
  QString prettyShortName() const override { return Metadata<PrettyName_k, Model>::get(); } \
  QString category() const override { return Metadata<Category_k, Model>::get(); } \
  QStringList tags() const override { return Metadata<Tags_k, Model>::get(); } \
