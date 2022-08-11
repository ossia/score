#pragma once

#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>

#include <score/statemachine/GraphicsSceneToolPalette.hpp>

namespace Library
{
struct ProcessData;
}
namespace Scenario
{
class ScenarioDocumentPresenter;
class CentralIntervalDisplay
{
public:
  explicit CentralIntervalDisplay(ScenarioDocumentPresenter& p);
  CentralIntervalDisplay(const CentralIntervalDisplay&) = delete;
  CentralIntervalDisplay(CentralIntervalDisplay&&) noexcept = delete;
  CentralIntervalDisplay& operator=(const CentralIntervalDisplay&) = delete;
  CentralIntervalDisplay& operator=(CentralIntervalDisplay&&) noexcept = delete;

  ~CentralIntervalDisplay();

  // Init method necessary because of
  // https://stackoverflow.com/questions/69050714/observing-the-state-of-a-variant-during-construction
  void init();

  void on_addProcessFromLibrary(const Library::ProcessData& dat);
  void on_addPresetFromLibrary(const Process::Preset& dat);
  void on_visibleRectChanged(const QRectF&);
  void on_executionTimer();

  ScenarioDocumentPresenter& parent;
  DisplayedElementsPresenter presenter;

private:
  std::unique_ptr<GraphicsSceneToolPalette> m_stateMachine;
};
}
