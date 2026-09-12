#pragma once

#include <Gris/Algo/SpeakerSetup.hpp>
#include <Gris/Metadata.hpp>
#include <Gris/SpeakerSetupInlet.hpp>

#include <Process/Process.hpp>

#include <score_plugin_gris_export.h>

#include <verdigris>

namespace Gris
{
/** Spatializes N input channels over the speakers of a SpatGRIS setup.
 *
 * Both VBAP and MBAP are resident and each source picks one, which is what
 * SpatGRIS's hybrid mode does. Gains never leave the process: the per-sample
 * interpolated mix-accumulate happens here and the output is ordinary audio,
 * so score's mixer provides everything SpatGRIS layers on top (mute/solo,
 * direct outs, master gain).
 */
class SCORE_PLUGIN_GRIS_EXPORT SpatModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(SpatModel)
  W_OBJECT(SpatModel)

public:
  /** Ports before the per-source ones. */
  enum FixedInlet
  {
    AudioIn = 0,
    SpeakerSetupPort,
    Interpolation,
    FixedInletCount
  };
  /** Ports per source, in order. */
  enum SourceInlet
  {
    Position = 0,
    AzimuthSpan,
    ZenithSpan,
    Mode,
    SourceInletCount
  };

  static constexpr int defaultSourceCount = 8;
  static constexpr int maxSourceCount = 128;

  explicit SpatModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);
  ~SpatModel() override;

  template <typename Impl>
  SpatModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  [[nodiscard]] SpeakerSetupInlet& speakerSetupInlet() const noexcept;
  [[nodiscard]] SpeakerSetup speakerSetup() const noexcept;

  [[nodiscard]] int sourceCount() const noexcept { return m_sourceCount; }
  /** Grows or shrinks the per-source ports, keeping the ones already there so
   *  that cables and automations on existing sources survive. Goes through
   *  Gris::SetSourceCount rather than being called directly, so it lands on
   *  the undo stack. */
  void setSourceCount(int count);
  W_SLOT(setSourceCount);
  void sourceCountChanged(int count) W_SIGNAL(sourceCountChanged, count);

  /** Index of the first inlet belonging to `source`. */
  [[nodiscard]] static int firstInletOf(int source) noexcept
  {
    return FixedInletCount + source * SourceInletCount;
  }

  PROPERTY(
      int, sourceCount READ sourceCount WRITE setSourceCount NOTIFY sourceCountChanged)

private:
  void init();
  /** Appends the four ports of one source. */
  void addSourcePorts(int source, int& nextId);

  int m_sourceCount{defaultSourceCount};
};
} // namespace Gris
