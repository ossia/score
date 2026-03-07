#pragma once
#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/Metadata.hpp>
#include <ClipLauncher/SceneModel.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/TexturePort.hpp>

#include <score/model/EntityMap.hpp>

#include <verdigris>

namespace ClipLauncher
{

class ProcessModel final : public Process::ProcessModel
{
  W_OBJECT(ProcessModel)
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(ClipLauncher::ProcessModel)

public:
  std::unique_ptr<Process::AudioInlet> inlet;
  std::unique_ptr<Process::AudioOutlet> outlet;
  std::unique_ptr<Gfx::TextureOutlet> textureOutlet;
  std::unique_ptr<Process::MidiOutlet> midiOutlet;

  score::EntityMap<LaneModel> lanes;
  score::EntityMap<SceneModel> scenes;
  score::EntityMap<CellModel> cells;

  ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx, QObject* parent);

  template <typename Impl>
  ProcessModel(Impl& vis, const score::DocumentContext& ctx, QObject* parent)
      : Process::ProcessModel{vis, parent}
      , m_context{ctx}
  {
    vis.writeTo(*this);
    init();
  }

  ~ProcessModel() override;

  const score::DocumentContext& context() const noexcept { return m_context; }

  // Cell lookup
  CellModel* cellAt(int lane, int scene) const;
  std::vector<CellModel*> cellsInLane(int lane) const;
  std::vector<CellModel*> cellsInScene(int scene) const;

  // Grid dimensions
  int laneCount() const noexcept { return lanes.size(); }
  int sceneCount() const noexcept { return scenes.size(); }

  // Global quantization
  double globalQuantization() const noexcept { return m_globalQuantization; }
  void setGlobalQuantization(double q);

  // Create a cell with proper clip launcher defaults
  static CellModel* createDefaultCell(
      const Id<CellModel>& id, int lane, int scene,
      const score::DocumentContext& ctx, QObject* parent);

  // ProcessModel overrides
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;
  void setSelection(const Selection& s) const noexcept override;

  // Signals
  void globalQuantizationChanged(double q)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, globalQuantizationChanged, q)

private:
  void init();
  const score::DocumentContext& m_context;
  double m_globalQuantization{1.0}; // Default: quantize to bar
};

using ProcessFactory = Process::ProcessFactory_T<ClipLauncher::ProcessModel>;

} // namespace ClipLauncher
