#pragma once
#include <Process/ProcessFlags.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/tools/Metadata.hpp>
#include <QObject>
#include <QStringList>
#include <cinttypes>
#define PROCESS_FLAGS_METADATA(Export, Model, Flags) \
  TYPED_METADATA(                                    \
      Export, Model, Process::ProcessFlags_k, Process::ProcessFlags, Flags)

#define PROCESS_METADATA(                                                \
    Export, Model, Uuid, ObjectKey, PrettyName, Category, Tags, Flags)   \
  MODEL_METADATA(                                                        \
      Export, Process::ProcessModel, Model, Uuid, ObjectKey, PrettyName) \
  CATEGORY_METADATA(Export, Model, Category)                             \
  TAGS_METADATA(Export, Model, Tags)                                     \
  PROCESS_FLAGS_METADATA(Export, Model, (Process::ProcessFlags)(Flags))

#define PROCESS_METADATA_IMPL(Model)                        \
  MODEL_METADATA_IMPL(Model)                                \
  QString prettyShortName() const override                  \
  {                                                         \
    return Metadata<PrettyName_k, Model>::get();            \
  }                                                         \
  QString category() const override                         \
  {                                                         \
    return Metadata<Category_k, Model>::get();              \
  }                                                         \
  QStringList tags() const override                         \
  {                                                         \
    return Metadata<Tags_k, Model>::get();                  \
  }                                                         \
  Process::ProcessFlags flags() const override              \
  {                                                         \
    return Metadata<Process::ProcessFlags_k, Model>::get(); \
  }
