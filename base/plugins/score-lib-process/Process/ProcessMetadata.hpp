#pragma once
#include <score/tools/Metadata.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <cinttypes>
#include <QObject>
#include <QStringList>
namespace Process
{
class ProcessModel;
class ProcessModelFactory;
class LayerFactory;
class EffectFactory;
enum ProcessFlags: int8_t
{
  SupportsTemporal = 0x1,
  SupportsEffectChain = 0x2,
  SupportsState = 0x4,
  RequiresCustomData = 0x8,
  PutInNewSlot = 0x16,

  SupportsLasting = SupportsTemporal | SupportsEffectChain,
  ExternalEffect = SupportsLasting | RequiresCustomData,
  SupportsAll = SupportsTemporal | SupportsEffectChain | SupportsState,
};

/**
 * \class ProcessFlags_k
 * \brief Metadata to retrieve the ProcessFlags of a process
 */

class ProcessFlags_k;
}

#define PROCESS_FLAGS_METADATA(Export, Model, Flags) \
  TYPED_METADATA(Export, Model, Process::ProcessFlags_k, Process::ProcessFlags, Flags)

#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName, Category, Tags, Flags) \
  MODEL_METADATA(                                                                           \
      Export, Process::ProcessModel, Model, Uuid, ObjectKey,                                \
      PrettyName)                                                                           \
  CATEGORY_METADATA(Export, Model, Category)                                                \
  TAGS_METADATA(Export, Model, Tags)                                                        \
  PROCESS_FLAGS_METADATA(Export, Model, (Process::ProcessFlags)(Flags))

#define PROCESS_METADATA_IMPL(Model) \
  MODEL_METADATA_IMPL(Model) \
  QString prettyShortName() const override { return Metadata<PrettyName_k, Model>::get(); } \
  QString category() const override { return Metadata<Category_k, Model>::get(); } \
  QStringList tags() const override { return Metadata<Tags_k, Model>::get(); }
