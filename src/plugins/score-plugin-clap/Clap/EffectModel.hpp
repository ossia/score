#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QSocketNotifier>

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
class Window;
class Model;
struct PluginHandle
{
  PluginHandle();
  PluginHandle(const PluginHandle&) = delete;
  PluginHandle(PluginHandle&&) = delete;
  PluginHandle& operator=(const PluginHandle&) = delete;
  PluginHandle& operator=(PluginHandle&&) = delete;
  ~PluginHandle();

  void load(Model& context, QByteArray path, QByteArray id);
  void* library{};
  const clap_plugin_entry_t* entry{};
  const clap_plugin_factory_t* factory{};
  const clap_plugin_t* plugin{};
  const clap_plugin_descriptor_t* desc{};

  clap_host_t host;

  // Common plugin extensions
  const clap_plugin_params_t* ext_params{};
  const clap_plugin_timer_support_t* ext_timer_support{};

  bool activated{};
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
  
  void closeUI() const;

  const QString& pluginId() const noexcept { return m_pluginId; }
  bool supports64() const noexcept { return m_supports64; }
  bool executing() const noexcept { return m_executing; }

  const std::vector<clap_param_info_t>& parameterInputs() const noexcept
  {
    return m_parameters_ins;
  }
  const std::vector<clap_param_info_t>& parameterOutputs() const noexcept
  {
    return m_parameters_outs;
  }
  const std::vector<clap_note_port_info_t>& midiInputs() const noexcept
  {
    return m_midi_ins;
  }
  const std::vector<clap_note_port_info_t>& midiOutputs() const noexcept
  {
    return m_midi_outs;
  }
  const std::vector<clap_audio_port_info_t>& audioInputs() const noexcept
  {
    return m_audio_ins;
  }
  const std::vector<clap_audio_port_info_t>& audioOutputs() const noexcept
  {
    return m_audio_outs;
  }

  // Preset functionality
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  std::vector<Process::Preset> builtinPresets() const noexcept override;

  void restartPlugin() W_SIGNAL(restartPlugin);
  Clap::Window* window{};
  std::vector<std::pair<clap_id, QTimer*>> timers;

  struct FdNotifiers
  {
    std::unique_ptr<QSocketNotifier> read;
    std::unique_ptr<QSocketNotifier> write;
    std::unique_ptr<QSocketNotifier> error;
  };
  ossia::hash_map<int, std::unique_ptr<FdNotifiers>> fd_notifiers;

private:
  void loadPlugin();
  void createControls(bool loading);
  void setupControlInlet(
      const clap_plugin_params_t&, const clap_param_info_t& info, int index,
      Process::ControlInlet* ctl);
  void setupControlOutlet(
      const clap_plugin_params_t&, const clap_param_info_t& info, int index,
      Process::ControlOutlet* ctl);

  QString m_pluginPath;
  QString m_pluginId;

  std::unique_ptr<PluginHandle> m_plugin;
  QByteArray m_loadedState;

  std::vector<clap_param_info_t> m_parameters_ins;
  std::vector<clap_param_info_t> m_parameters_outs;
  std::vector<clap_audio_port_info_t> m_audio_ins;
  std::vector<clap_audio_port_info_t> m_audio_outs;
  std::vector<clap_note_port_info_t> m_midi_ins;
  std::vector<clap_note_port_info_t> m_midi_outs;

  bool m_supports64{};
  bool m_executing{};
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
class Window;
using ProcessFactory = Process::EffectProcessFactory_T<Clap::Model>;
using EffectLayerFactory = Process::EffectLayerFactory_T<
    Clap::Model,
    Process::DefaultEffectItem,
    Window>;
}
