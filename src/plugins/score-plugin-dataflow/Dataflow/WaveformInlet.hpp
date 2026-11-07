#pragma once

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/Wavetable.hpp>

#include <Dataflow/PortItem.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <QGraphicsItem>
#include <QRectF>
#include <QString>

#include <optional>
#include <span>
#include <vector>

class QMenu;

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
//! observers receive valueChanged. setValue takes its canonical value as it
//! is, and any other shape execution takes (Process::waveformFrame) adapted
//! to the port: a flat list is one frame, a single frame port keeps the first
//! frame of a bank, samples are clamped, then resampled to the frame size
//! constraint and put on the amplitude levels. A value that holds no list of
//! numbers is ignored with a warning.
//!
//! The editor, in the scene and in the inspector, draws samples with optional
//! amplitude quantization; its context menu generates and transforms frames,
//! resamples them and adds, removes and reorders the frames of a bank, each
//! edit being an undoable SetControlValue command.
//!
//! Editor gestures: drag draws, Shift-drag draws a straight line; on a
//! continuous port the pen catches the zero, half and full amplitude lines
//! unless Alt is held. With keyboard focus (click the editor): Up and Down
//! move the hovered or last edited sample by one level (one 64th of the range
//! when continuous; Shift for an eighth), a held key being one undo step;
//! Left and Right rotate the frame, Page Up and Page Down choose the frame of
//! a bank, Delete clears, and Copy and Paste exchange the frame as text (see
//! waveformFrameText). In a bank's frame strip, drag a thumbnail to move the
//! frame, double-click to duplicate it, and use the wheel to choose.
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

//! What a waveform editor item currently shows, in item coordinates.
struct WaveformItemState
{
  //! Where samples are drawn and edited: one equal column per sample.
  QRectF plot;
  //! The frame chooser of a bank; null for a single frame.
  QRectF frames;
  std::size_t frame{};
  QString label;
  //! The hovered sample, or with focus the last edited one; empty when none is.
  QString readout;
  //! Execution values are shown in place of the document's frame.
  bool live{};
};
SCORE_PLUGIN_DATAFLOW_EXPORT
WaveformItemState waveformItemState(const QGraphicsItem& item);
//! The item's right-click menu: shapes, transforms, resampling and frames.
SCORE_PLUGIN_DATAFLOW_EXPORT
void fillWaveformItemMenu(QGraphicsItem& item, QMenu& menu);

//! A frame as text: its samples separated by spaces, as a list message reads
//! in Max, Pd or OSC tools, in the shortest form that reads back exactly.
SCORE_PLUGIN_DATAFLOW_EXPORT
QString waveformFrameText(std::span<const float> frame);
//! Reads numbers separated by spaces, commas or semicolons, in one pair of
//! brackets or none, as a multislider's value or a list message in Max or Pd;
//! anything else, such as a bank's nested lists, as State::parseValue reads a
//! value. The value then reads as execution reads a cable's
//! (Process::waveformFrame): a bank gives its first frame, amplitudes are
//! bipolar and clamped. The frame is resampled periodically to `size` samples
//! and put on the port's `levels`; a single number fills it. Empty when the
//! text holds anything but numbers.
SCORE_PLUGIN_DATAFLOW_EXPORT
std::optional<std::vector<float>>
waveformFrameFromText(QStringView text, std::size_t size, int levels);
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
