#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <clap/all.h>

#include <memory>
#include <verdigris>

namespace Clap
{
class Model;
}

PROCESS_METADATA(
    , Clap::Model, "10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f", "CLAP", "CLAP",
    Process::ProcessCategory::Other, "Plugins", "CLever Audio Plug-in", "ossia score",
    {}, {}, {},
    QUrl(
        "https://ossia.io/score-docs/processes/"
        "audio-plugins.html#common-formats-vst-vst3-lv2-jsfx"),
    Process::ProcessFlags::ExternalEffect)
DESCRIPTION_METADATA(, Clap::Model, "Clap")

namespace Clap
{

struct ParameterInfo
{
  clap_id param_id;
  double min_value, max_value, default_value;
};

struct PluginHandle
{
  PluginHandle();
  PluginHandle(const PluginHandle&) = delete;
  PluginHandle(PluginHandle&&) = delete;
  PluginHandle& operator=(const PluginHandle&) = delete;
  PluginHandle& operator=(PluginHandle&&) = delete;
  ~PluginHandle();
  void* library{};
  const clap_plugin_entry_t* entry{};
  const clap_plugin_factory_t* factory{};
  const clap_plugin_t* plugin{};
  const clap_plugin_descriptor_t* desc{};

  clap_host_t host;
};

class Model final : public Process::ProcessModel
{
  W_OBJECT(Model)
  SCORE_SERIALIZE_FRIENDS

public:
  MODEL_METADATA_IMPL(Model)

  Model(
      const TimeVal& duration, const QString& pluginPath,
      const Id<Process::ProcessModel>& id, QObject* parent);

  Model(DataStream::Deserializer& vis, QObject* parent);
  Model(JSONObject::Deserializer& vis, QObject* parent);
  Model(DataStream::Deserializer&& vis, QObject* parent);
  Model(JSONObject::Deserializer&& vis, QObject* parent);

  ~Model() override;

  QString prettyShortName() const noexcept override
  {
    return Metadata<PrettyName_k, Model>::get();
  }
  QString category() const noexcept override
  {
    return Metadata<Category_k, Model>::get();
  }
  QStringList tags() const noexcept override { return Metadata<Tags_k, Model>::get(); }
  Process::ProcessFlags flags() const noexcept override;
  bool hasExternalUI() const noexcept;
  PluginHandle* handle() const noexcept { return m_plugin.get(); }

  const QString& pluginId() const noexcept { return m_pluginId; }
  bool supports64() const noexcept { return m_supports64; }
  
  const std::vector<ParameterInfo>& parameterInfo() const noexcept { return m_parameters; }
  const std::vector<clap_note_port_info_t>& midiInputs() const noexcept
  {
    return m_midi_inputs_info;
  }
  const std::vector<clap_note_port_info_t>& midiOutputs() const noexcept
  {
    return m_midi_outputs_info;
  }
  const std::vector<clap_audio_port_info_t>& audioInputs() const noexcept
  {
    return m_audio_inputs_info;
  }
  const std::vector<clap_audio_port_info_t>& audioOutputs() const noexcept
  {
    return m_audio_outputs_info;
  }

private:
  void reload();
  void loadPlugin();
  void unloadPlugin();
  void createControls();

  QString m_pluginPath;
  QString m_pluginId;

  std::unique_ptr<PluginHandle> m_plugin;

  bool m_supports64{};
  std::vector<ParameterInfo> m_parameters;
  std::vector<clap_audio_port_info_t> m_audio_inputs_info;
  std::vector<clap_audio_port_info_t> m_audio_outputs_info;
  std::vector<clap_note_port_info_t> m_midi_inputs_info;
  std::vector<clap_note_port_info_t> m_midi_outputs_info;
};
}

namespace Process
{
template <>
QString EffectProcessFactory_T<Clap::Model>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<Clap::Model>::descriptor(QString d) const noexcept;
}

namespace Clap
{
using ProcessFactory = Process::EffectProcessFactory_T<Clap::Model>;
// using EffectLayerFactory = Process::EffectLayerFactory_T<
//     Clap::Model,
//     Process::DefaultEffectItem,
//     Process::ProcessModel>;
}
