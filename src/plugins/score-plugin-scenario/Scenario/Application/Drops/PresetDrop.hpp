#pragma once
#include <score_plugin_scenario_export.h>

#include <functional>
#include <optional>
#include <vector>

namespace Process
{
class ProcessModel;
}

namespace Scenario
{
enum class PresetDrop
{
  Controls,
  ControlsAndState,
  Copy
};

struct PresetDropChoices
{
  bool controls{};
  bool controlsAndState{};
  bool copy{};
};

//! withState serializes the processes to find whether they have a state.
SCORE_PLUGIN_SCENARIO_EXPORT PresetDropChoices presetDropChoices(
    const std::vector<const Process::ProcessModel*>& processes, bool copy,
    bool withState = true);

//! With Ctrl held, a menu at the cursor lists the choices; empty if dismissed.
SCORE_PLUGIN_SCENARIO_EXPORT std::optional<PresetDrop> choosePresetDrop(
    const std::vector<const Process::ProcessModel*>& processes, bool copy);

//! Replaces choosePresetDrop when set
SCORE_PLUGIN_SCENARIO_EXPORT extern std::function<std::optional<PresetDrop>(
    const PresetDropChoices&)>
    presetDropChooser;
}
