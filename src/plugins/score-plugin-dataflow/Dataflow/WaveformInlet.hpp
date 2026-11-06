#pragma once

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/Wavetable.hpp>

#include <Dataflow/PortItem.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <QGraphicsItem>

namespace Dataflow
{
struct WaveformInlet;
struct WavetableInlet;
}

UUID_METADATA(
    SCORE_PLUGIN_DATAFLOW_EXPORT, Process::Port, Dataflow::WaveformInlet,
    "be15516a-a8be-490c-b3a8-82b25f985096")
UUID_METADATA(
    SCORE_PLUGIN_DATAFLOW_EXPORT, Process::Port, Dataflow::WavetableInlet,
    "c2ee4fdc-9a49-4818-965c-cf5bf98cfa49")

namespace Dataflow
{
//! A single periodic frame of samples for sample-backed oscillators; see
//! WavetableInlet for an ordered bank of frames.
//!
//! The value is the canonical form of Process::WavetableData (a list of frame
//! lists) and is the only persisted content; wavetable() is updated before
//! observers receive valueChanged. Values the port cannot hold are ignored.
//!
//! The editor, in the scene and in the inspector, draws samples with optional
//! amplitude quantization, and adds, removes, reorders and resamples frames,
//! each edit being an undoable SetControlValue command.
//!
//! Like any control inlet, these ports take cables and addresses and are
//! registered for execution: a list of numbers arriving there is one frame,
//! a list of lists a bank (Process::waveformFrame). A node resamples what it
//! receives into its fixed native arrays with Process::prepareWaveform, which
//! reads the value in place. Values execution sends back to the UI through
//! setExecutionValue are drawn over the edited frame, as other controls show
//! theirs, without changing the document.
struct SCORE_PLUGIN_DATAFLOW_EXPORT WaveformInlet : Process::ControlInlet
{
  W_OBJECT(WaveformInlet)
public:
  SCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(WaveformInlet)
  WaveformInlet(Id<Process::Port> id, QObject* parent);
  WaveformInlet(DataStream::Deserializer& vis, QObject* parent);
  WaveformInlet(JSONObject::Deserializer& vis, QObject* parent);
  WaveformInlet(DataStream::Deserializer&& vis, QObject* parent);
  WaveformInlet(JSONObject::Deserializer&& vis, QObject* parent);
  ~WaveformInlet() override;

  void setValue(const ossia::value& value) override;
  void setWaveform(const Process::WavetableData& waveform);
  const Process::WavetableData& wavetable() const noexcept { return m_waveform; }

  //! Zero means continuous; otherwise painting and preview use this many levels.
  void setAmplitudeLevels(int levels);
  int amplitudeLevels() const noexcept { return m_amplitudeLevels; }
  //! Zero permits explicit resampling to any supported frame size.
  void setFrameSizeConstraint(std::size_t size);
  std::size_t frameSizeConstraint() const noexcept { return m_frameSizeConstraint; }
  bool supportsFrames() const noexcept { return m_bank; }
  void constraintsChanged() W_SIGNAL(constraintsChanged);

protected:
  WaveformInlet(Id<Process::Port> id, QObject* parent, bool bank);
  WaveformInlet(DataStream::Deserializer& vis, QObject* parent, bool bank);
  WaveformInlet(JSONObject::Deserializer& vis, QObject* parent, bool bank);

private:
  void initializeLoadedValue();
  bool accepts(const Process::WavetableData& data) const noexcept;

  Process::WavetableData m_waveform{Process::makeTriangleWavetable()};
  int m_amplitudeLevels{};
  std::size_t m_frameSizeConstraint{};
  bool m_bank{};
};

//! A bank of equal-size periodic frames. The frame being edited is a property
//! of the editor, not of the port.
struct SCORE_PLUGIN_DATAFLOW_EXPORT WavetableInlet final : WaveformInlet
{
  MODEL_METADATA_IMPL(WavetableInlet)
  WavetableInlet(Id<Process::Port> id, QObject* parent);
  WavetableInlet(DataStream::Deserializer& vis, QObject* parent);
  WavetableInlet(JSONObject::Deserializer& vis, QObject* parent);
  WavetableInlet(DataStream::Deserializer&& vis, QObject* parent);
  WavetableInlet(JSONObject::Deserializer&& vis, QObject* parent);
};

SCORE_PLUGIN_DATAFLOW_EXPORT
void setWaveformItemModel(QGraphicsItem& item, WaveformInlet& model);
SCORE_PLUGIN_DATAFLOW_EXPORT
void setWaveformItemRect(QGraphicsItem& item, const QRectF& rect);
}

namespace WidgetFactory
{
struct SCORE_PLUGIN_DATAFLOW_EXPORT WaveformInletItems
{
  static constexpr Process::PortItemLayout layout() noexcept { return {}; }
  static QWidget* make_widget(
      const Dataflow::WaveformInlet& inlet, const score::DocumentContext& ctx,
      QWidget* parent, QObject* context);
  static QGraphicsItem* make_item(
      const Dataflow::WaveformInlet& slider, const Dataflow::WaveformInlet& inlet,
      const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
};
}
