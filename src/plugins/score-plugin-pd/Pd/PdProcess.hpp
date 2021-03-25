#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>

#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <Pd/PdMetadata.hpp>

namespace Pd
{
class ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Pd::ProcessModel)
  W_OBJECT(ProcessModel)

public:
  using base_type = Process::ProcessModel;
  explicit ProcessModel(
      const TimeVal& duration,
      const QString& pdpatch,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  void setScript(const QString& script);
  const QString& script() const;

  ~ProcessModel() override;

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
      int,
      audioInputs READ audioInputs WRITE setAudioInputs NOTIFY
          audioInputsChanged)
  W_PROPERTY(
      int,
      audioOutputs READ audioOutputs WRITE setAudioOutputs NOTIFY
          audioOutputsChanged)
  W_PROPERTY(
      bool,
      midiInput READ midiInput WRITE setMidiInput NOTIFY midiInputChanged)
  W_PROPERTY(
      bool,
      midiOutput READ midiOutput WRITE setMidiOutput NOTIFY midiOutputChanged)

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString m_script;
  int m_audioInputs{0};
  int m_audioOutputs{0};
  bool m_midiInput{};
  bool m_midiOutput{};
};
}
