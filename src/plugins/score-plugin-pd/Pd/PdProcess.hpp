#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>

#include <Pd/PdInstance.hpp>
#include <Pd/PdMetadata.hpp>

#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
namespace Pd
{

const QString& locatePdBinary() noexcept;

struct PatchSpec
{
  struct Control
  {
    QString name;   // Pretty name
    QString remote; // Name used for send/receive
    QString type;
    QString widget;
    QString unit;
    ossia::value defaultv;
    ossia::domain domain;
    std::optional<ossia::val_type> deduced_type;
  };
  std::vector<Control> receives, sends;
};

class ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Pd::ProcessModel)
  W_OBJECT(ProcessModel)

public:
  using score_base_type = Process::ProcessModel;
  explicit ProcessModel(
      const TimeVal& duration, const QString& pdpatch,
      const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    init();
    vis.writeTo(*this);
  }

  bool hasExternalUI() const noexcept;

  void setScript(const QString& script);
  const QString& script() const;

  ~ProcessModel() override;

  const PatchSpec& patchSpec() const noexcept { return m_spec; }
  int audioInputs() const;
  int audioOutputs() const;
  bool midiInput() const;
  bool midiOutput() const;

  void setAudioInputs(int audioInputs);
  void setAudioOutputs(int audioOutputs);
  void setMidiInput(bool midiInput);
  void setMidiOutput(bool midiOutput);

  void scriptChanged(QString v) W_SIGNAL(scriptChanged, v);
  void audioInputsChanged(int v) W_SIGNAL(audioInputsChanged, v);
  void audioOutputsChanged(int v) W_SIGNAL(audioOutputsChanged, v);
  void midiInputChanged(bool v) W_SIGNAL(midiInputChanged, v);
  void midiOutputChanged(bool v) W_SIGNAL(midiOutputChanged, v);

  W_PROPERTY(
      int, audioInputs READ audioInputs WRITE setAudioInputs NOTIFY audioInputsChanged)
  W_PROPERTY(
      int,
      audioOutputs READ audioOutputs WRITE setAudioOutputs NOTIFY audioOutputsChanged)
  W_PROPERTY(bool, midiInput READ midiInput WRITE setMidiInput NOTIFY midiInputChanged)
  W_PROPERTY(
      bool, midiOutput READ midiOutput WRITE setMidiOutput NOTIFY midiOutputChanged)

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
  std::shared_ptr<Instance> m_instance;

private:
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  void init();
  QString m_script;
  PatchSpec m_spec;
  int m_audioInputs{0};
  int m_audioOutputs{0};
  bool m_midiInput{};
  bool m_midiOutput{};
};
}
