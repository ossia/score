#pragma once
#include <Process/Dataflow/PortType.hpp>
#include <Process/ProcessFlags.hpp>

#include <score/plugins/SerializableInterface.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/std/Optional.hpp>

#include <QStringList>

#include <cinttypes>

namespace Process
{
SCORE_LIB_PROCESS_EXPORT
const QIcon& getCategoryIcon(const QString& category) noexcept;
enum ProcessCategory
{
  Other,
  Automation,
  Generator,   // lfo, etc
  MediaSource, // sound, video, image, etc
  Analyzer,
  AudioEffect, // audio in and audio out
  MidiEffect,  // midi in and midi out
  Synth,       // midi in and audio out
  Mapping,     // value in and value out
  Script,      // JS, PD, etc
  Structure,   // scenario, loop, etc
  Visual,      // gfx processes
};

struct Descriptor
{
  QString prettyName;
  ProcessCategory category{};
  QString categoryText;
  QString description;
  QString author{"ossia score"};
  QStringList tags;
  std::optional<std::vector<Process::PortType>> inlets;
  std::optional<std::vector<Process::PortType>> outlets;
};
class Descriptor_k;
}
#define PROCESS_FLAGS_METADATA(Export, Model, Flags) \
  TYPED_METADATA(Export, Model, Process::ProcessFlags_k, Process::ProcessFlags, Flags)

#define PROCESS_METADATA(                                                                 \
    Export,                                                                               \
    Model,                                                                                \
    Uuid,                                                                                 \
    ObjectKey,                                                                            \
    PrettyName,                                                                           \
    CategoryEnum,                                                                         \
    Category,                                                                             \
    Desc,                                                                                 \
    Author,                                                                               \
    Tags,                                                                                 \
    InputSpec,                                                                            \
    OutputSpec,                                                                           \
    Flags)                                                                                \
  MODEL_METADATA(Export, Process::ProcessModel, Model, Uuid, ObjectKey, PrettyName)       \
  CATEGORY_METADATA(Export, Model, Category)                                              \
  TAGS_METADATA(Export, Model, Tags)                                                      \
  PROCESS_FLAGS_METADATA(Export, Model, (Process::ProcessFlags)(Flags))                   \
  template <>                                                                             \
  struct Export Metadata<::Process::Descriptor_k, Model>                                  \
  {                                                                                       \
    static ::Process::Descriptor get()                                                    \
    {                                                                                     \
      static const ::Process::Descriptor k{                                               \
          PrettyName, CategoryEnum, Category, Desc, Author, Tags, InputSpec, OutputSpec}; \
      return k;                                                                           \
    }                                                                                     \
  };

#define PROCESS_METADATA_IMPL(Model)                                                        \
  MODEL_METADATA_IMPL(Model)                                                                \
  QString prettyShortName() const noexcept override                                         \
  {                                                                                         \
    return Metadata<PrettyName_k, Model>::get();                                            \
  }                                                                                         \
  QString category() const noexcept override { return Metadata<Category_k, Model>::get(); } \
  QStringList tags() const noexcept override { return Metadata<Tags_k, Model>::get(); }     \
  Process::ProcessFlags flags() const noexcept override                                     \
  {                                                                                         \
    return Metadata<Process::ProcessFlags_k, Model>::get();                                 \
  }
