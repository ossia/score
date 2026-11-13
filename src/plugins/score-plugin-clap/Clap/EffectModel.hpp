#pragma once
#include <optional>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#include <atomic>

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
struct PluginHandle;

//! One instance of a plug-in, with the host it is given, and where it is in
//! the life cycle of clap/plugin.h: activate and deactivate are
//! [main-thread], start_processing and stop_processing [audio-thread]. The
//! main thread alone moves it between inactive and active, the audio thread
//! alone between active and processing; each claims it before calling it, so
//! that neither calls it in a state the other is changing.
struct PluginInstance : std::enable_shared_from_this<PluginInstance>
{
  enum State : uint8_t
  {
    Inactive,
    Active,
    Processing,
    //! Claimed by a thread calling it
    Busy
  };

  PluginInstance();
  PluginInstance(const PluginInstance&) = delete;
  PluginInstance(PluginInstance&&) = delete;
  PluginInstance& operator=(const PluginInstance&) = delete;
  PluginInstance& operator=(PluginInstance&&) = delete;
  ~PluginInstance();

  //! What the plug-in calls back: its host_data is this instance
  clap_host_t host{};
  const clap_plugin_t* plugin{};
  //! The handle of its process, which has its library and its model
  PluginHandle* owner{};

  //! Queried on the main thread: the audio thread uses them as they are
  const clap_plugin_params_t* ext_params{};
  const clap_plugin_thread_pool_t* ext_thread_pool{};

  std::atomic<uint8_t> state{Inactive};
  //! A restart is to be done: it does not start processing until it is
  std::atomic_bool restart_pending{};
  //! It asked to process while it sleeps
  std::atomic_bool wake{};

  //! [main-thread] The nodes that run it, and the rate and block size the
  //! last of them wants it active with
  int nodes{};
  double node_rate{};
  uint32_t node_block{};

  // [main-thread]
  bool activate(double rate, uint32_t block);
  //! Not while it is processing or claimed by the audio thread
  bool deactivate();
  //! Active while a node holds it, at that node's rate, once the restarts
  //! asked for are done; inactive otherwise. What processes it is stopped
  //! first by its node, which calls this again.
  void reconcile();
  //! Deactivated and destroyed: it is not processing any more
  void destroy();

  // [audio-thread]
  //! Reset, then started, if active and with no restart to do
  bool start_processing();
  void stop_processing();
};

//! The first instance of a process's plug-in, and the library it comes from
struct PluginHandle : PluginInstance
{
  PluginHandle();
  ~PluginHandle();

  void load(Model& context, QByteArray path, QByteArray id);
  void* library{};
  const clap_plugin_entry_t* entry{};
  const clap_plugin_factory_t* factory{};
  const clap_plugin_descriptor_t* desc{};

  // Common plugin extensions
  const clap_plugin_timer_support_t* ext_timer_support{};

  // Metadata we parsed
  std::vector<clap_param_info_t> m_parameters_ins;
  std::vector<clap_param_info_t> m_parameters_outs;
  std::vector<clap_audio_port_info_t> m_audio_ins;
  std::vector<clap_audio_port_info_t> m_audio_outs;
  std::vector<clap_note_port_info_t> m_midi_ins;
  std::vector<clap_note_port_info_t> m_midi_outs;

  ossia::flat_map<clap_id, std::uint32_t> param_outs_by_id;

  QPointer<Model> model;
};

//! [main-thread] What a plug-in that stops processing on another thread than
//! the audio one is told by thread-check: clap/ext/thread-check.h, "The host
//! may mark any OS thread, including the main-thread as the audio-thread, as
//! long as it can guarantee that only one OS thread is the audio-thread at a
//! time in a plugin instance."
struct ActingAsAudioThread
{
  ActingAsAudioThread() noexcept;
  ~ActingAsAudioThread();
  ActingAsAudioThread(const ActingAsAudioThread&) = delete;
  ActingAsAudioThread& operator=(const ActingAsAudioThread&) = delete;
  static bool active() noexcept;

private:
  bool m_previous{};
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

  //! "<path>:::<id>", what the factory makes this plug-in from
  QString effect() const noexcept override;

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
  const std::shared_ptr<PluginHandle>& handle() const noexcept { return m_plugin; }

  void closeUI() const;

  const QString& pluginId() const noexcept { return m_pluginId; }
  bool supports64() const noexcept { return m_supports64; }

  void mapExternalFiles(Process::ExternalFileMap& map) override;

  const std::vector<clap_param_info_t>& parameterInputs() const noexcept
  {
    return m_plugin->m_parameters_ins;
  }
  const std::vector<clap_param_info_t>& parameterOutputs() const noexcept
  {
    return m_plugin->m_parameters_outs;
  }
  const std::vector<clap_note_port_info_t>& midiInputs() const noexcept
  {
    return m_plugin->m_midi_ins;
  }
  const std::vector<clap_note_port_info_t>& midiOutputs() const noexcept
  {
    return m_plugin->m_midi_outs;
  }
  const std::vector<clap_audio_port_info_t>& audioInputs() const noexcept
  {
    return m_plugin->m_audio_ins;
  }
  const std::vector<clap_audio_port_info_t>& audioOutputs() const noexcept
  {
    return m_plugin->m_audio_outs;
  }

  // Preset functionality
  void loadPreset(const Process::Preset& preset) override;
  //! Its ports are made once, from the plug-in: a state sets parameter
  //! values, and a parameter layout changing at runtime does not rebuild them
  bool presetChangesPorts(const Process::Preset&) const noexcept override
  {
    return false;
  }
  //! Also a preset that does not name its plug-in, as any of them may have
  //! saved it
  bool presetMatches(const Process::Preset& preset) const noexcept override;
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

  void requestFlush() W_SIGNAL(requestFlush);

  void flushFromPluginToHost();

  bool currentlyReadingValues{};

  void markStateDirty();
  void stateSnapshotChanged(const QByteArray& blob)
      W_SIGNAL(stateSnapshotChanged, blob);

  void onParamRescan(clap_param_rescan_flags flags);
  //! The plug-in was deactivated: what waited for it is done
  void onDeactivated();

private:
  void loadPlugin();
  void createControls(bool loading);

  void refreshParamValues();
  //! The plug-in's presets, from its preset-discovery providers
  std::vector<Process::Preset> discoverPresets() const noexcept;
  mutable std::optional<std::vector<Process::Preset>> m_builtinPresets;
  // Returns false on structural mismatch — caller should treat as RESCAN_ALL.
  bool refreshParamInfoIfStable();
  void setupControlInlet(
      const clap_plugin_params_t&, const clap_param_info_t& info, int index,
      Process::ControlInlet* ctl);
  void setupControlOutlet(
      const clap_plugin_params_t&, const clap_param_info_t& info, int index,
      Process::ControlOutlet* ctl);

  void sendParamChange(const clap_param_info_t& info, double value);

  QTimer* m_stateDirtyTimer{};

  QString m_pluginPath;
  QString m_pluginId;

  std::shared_ptr<PluginHandle> m_plugin;
  QByteArray m_loadedState;

  bool m_supports64{};

  clap_param_rescan_flags m_pendingRescanFlags{};
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
using EffectLayerFactory = Process::EffectLayerFactory_T<Clap::Model, Window>;
}
