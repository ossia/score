#pragma once
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Vst/Loader.hpp>

#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/hash_map.hpp>

#include <atomic>
#include <memory>

#include <score_plugin_vst_export.h>

#include <verdigris>
namespace vst
{
class Model;
class ControlInlet;
}
PROCESS_METADATA(
    , vst::Model, "BE8E6BD3-75F2-4102-8895-8A4EB4EA545A", "VST", "VST",
    Process::ProcessCategory::Other, "Plugins", "VST plug-in",
    "VST is a trademark of Steinberg Media Technologies GmbH", {}, {}, {},
    QUrl("https://ossia.io/score-docs/processes/audio-plugins.html#controlling-vst-parameters"),
    Process::ProcessFlags::ExternalEffect)
UUID_METADATA(, Process::Port, vst::ControlInlet, "e523bc44-8599-4a04-94c1-04ce0d1a692a")
DESCRIPTION_METADATA(, vst::Model, "")
namespace vst
{
#define VST_FIRST_CONTROL_INDEX(synth) ((synth) ? 2 : 1)
struct SCORE_PLUGIN_VST_EXPORT AEffectWrapper
{
  AEffect* fx{};
  //! What audioMasterGetTime answers. Until the process plays, and once it
  //! has stopped: a transport stopped at 0, at 120 BPM in 4/4.
  VstTimeInfo info{
      .samplePos = 0.,
      .sampleRate = 44100.,
      .nanoSeconds = 0.,
      .ppqPos = 0.,
      .tempo = 120.,
      .barStartPos = 0.,
      .cycleStartPos = 0.,
      .cycleEndPos = 0.,
      .timeSigNumerator = 4,
      .timeSigDenominator = 4,
      .smpteOffset = 0,
      .smpteFrameRate = 0,
      .samplesToNextClock = 0,
      .flags = kVstPpqPosValid | kVstTempoValid | kVstBarsValid | kVstTimeSigValid};
  bool ui_opened{};

  AEffectWrapper(AEffect* f) noexcept
      : fx{f}
  {
  }

  auto getParameter(int32_t index) const noexcept { return fx->getParameter(fx, index); }
  auto setParameter(int32_t index, float p) const noexcept
  {
    return fx->setParameter(fx, index, p);
  }

  auto dispatch(
      int32_t opcode, int32_t index = 0, intptr_t value = 0, void* ptr = nullptr,
      float opt = 0.0f) const noexcept
  {
    return fx->dispatcher(fx, opcode, index, value, ptr, opt);
  }

  ~AEffectWrapper();
};

class CreateControl;
class ControlInlet;
class SCORE_PLUGIN_VST_TEST_EXPORT Model final : public Process::ProcessModel
{
  W_OBJECT(Model)
  SCORE_SERIALIZE_FRIENDS
  friend class vst::CreateControl;

public:
  MODEL_METADATA_IMPL(Model)
  Model(
      TimeVal t, const QString& name, const Id<Process::ProcessModel>&, QObject* parent);

  ~Model() override;
  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : ProcessModel{vis, parent}
      , m_registration{*this}
  {
    init();
    vis.writeTo(*this);
  }

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
  void setCreatingControls(bool ok) override;

  ControlInlet* getControl(const Id<Process::Port>& p) const;
  QString effect() const noexcept override;
  QString prettyName() const noexcept override;
  bool hasExternalUI() const noexcept;

  //! Whether the process has a MIDI input, after its audio input. A process
  //! is made with one for a plug-in flagged as a synth or that can receive
  //! MIDI events; a saved process keeps the ports it was saved with.
  bool takesMidi() const noexcept;

  //! Whether the process has a MIDI output, after its audio output: made for
  //! a plug-in that can send MIDI events.
  bool sendsMidi() const noexcept;

  std::shared_ptr<AEffectWrapper> fx{};

  ossia::hash_map<int, ControlInlet*> controls;

  void removeControl(const Id<Process::Port>&);
  void removeControl(int fxnum);

  //void addControl(int idx, float v) W_SIGNAL(addControl, idx, v);
  void on_addControl(int idx, float v);
  W_SLOT(on_addControl);
  void on_addControl_impl(ControlInlet* inl);
  void on_controlChangedFromScore(int num, float newval);

  void reloadControls();
  void reloadPrograms();

  //! The plug-in says it changed a parameter (audioMasterAutomate). Callable
  //! from any thread, the audio one included: it neither allocates nor locks.
  void parameterChangedByPlugin(int32_t index) noexcept;

  //! Moves the control of each parameter the plug-in said it changed to the
  //! value the plug-in has for it now, without sending it back to the
  //! plug-in; a parameter without a control gets one while the process
  //! creates them. Main thread.
  void updateControlsChangedByPlugin();

  auto dispatch(
      int32_t opcode, int32_t index = 0, intptr_t value = 0, void* ptr = nullptr,
      float opt = 0.0f)
  {
    return fx->dispatch(opcode, index, value, ptr, opt);
  }

  //! Selects a program of the plug-in, between effBeginSetProgram and
  //! effEndSetProgram
  void selectProgram(int index);

  std::atomic_bool needIdle{};

  //! Emitted on the main thread with the value the plug-in has for a
  //! parameter, before its control takes it
  void controlReadFromPlugin(int32_t fxNum, float value)
      W_SIGNAL(controlReadFromPlugin, fxNum, value);

  //! State saved with a plug-in that could not be loaded, written back as is
  //! by the serializers (JSON "Data": base64 chunk, "Params": values,
  //! "Program": the program it was on, -1 for none).
  std::string m_backup_chunk;
  ossia::float_vector m_backup_float_data;
  int32_t m_backup_program{-1};

private:
  void loadPreset(const Process::Preset& preset) override;
  //! Its ports are made once, from the plug-in; a program or a state may
  //! expose more of its parameters as controls, never fewer
  bool presetChangesPorts(const Process::Preset&) const noexcept override
  {
    return false;
  }
  Process::Preset savePreset() const noexcept override;
  std::vector<Process::Preset> builtinPresets() const noexcept override;

  QString getString(AEffectOpcodes op, int param);
  void setControlName(int fxnum, ControlInlet* ctrl);
  void init();
  void create();
  void load();
  void closePlugin();
  void initFx();

  std::vector<std::pair<std::string, int>> m_programs;
  int32_t m_effectId{};
  bool m_createControls{};

  //! One flag per parameter the plug-in had when it was opened, raised by
  //! parameterChangedByPlugin; m_otherParamChanged for an index past them,
  //! m_paramsChanged for any.
  std::unique_ptr<std::atomic_bool[]> m_changedParams;
  int32_t m_changedParamCount{};
  std::atomic_bool m_otherParamChanged{};
  std::atomic_bool m_paramsChanged{};
  //! A control is being moved to the plug-in's value: not sent back to it
  bool m_updatingFromPlugin{};

  struct vst_context_handler
  {
    Model& self;
    explicit vst_context_handler(Model& self);
    ~vst_context_handler();
  } m_registration;
};

// VSTModule* getPlugin(QString path);
AEffect* getPluginInstance(int32_t id);
//! What score answers a plug-in calling its host
SCORE_PLUGIN_VST_TEST_EXPORT
intptr_t vst_host_callback(
    AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr,
    float opt);
}

namespace Process
{
template <>
QString EffectProcessFactory_T<vst::Model>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<vst::Model>::descriptor(QString d) const noexcept;
}

template <>
SCORE_PLUGIN_VST_TEST_EXPORT void JSONWriter::write(vst::Model& eff);

namespace vst
{
using VSTEffectFactory = Process::EffectProcessFactory_T<Model>;
}
