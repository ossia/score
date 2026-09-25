#include "PresetDrop.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessState.hpp>

#include <QApplication>
#include <QCursor>
#include <QMenu>

namespace Scenario
{
std::function<std::optional<PresetDrop>(const PresetDropChoices&)> presetDropChooser;

PresetDropChoices presetDropChoices(
    const std::vector<const Process::ProcessModel*>& processes, bool copy,
    bool withState)
{
  PresetDropChoices res;
  res.copy = copy;
  for(auto p : processes)
  {
    bool controls = false;
    p->forEachControl(
        [&](Process::ControlInlet&, const ossia::value&) { controls = true; });
    res.controls |= controls;
    if(withState && !res.controlsAndState && !Process::stateBeyondControls(*p).isEmpty())
      res.controlsAndState = true;
  }
  return res;
}

std::optional<PresetDrop> choosePresetDrop(
    const std::vector<const Process::ProcessModel*>& processes, bool copy)
{
  if(presetDropChooser)
    return presetDropChooser(presetDropChoices(processes, copy));

  if(!(qApp->keyboardModifiers() & Qt::ControlModifier))
  {
    const auto choices = presetDropChoices(processes, copy, false);
    if(choices.controls)
      return PresetDrop::Controls;
    if(choices.copy)
      return PresetDrop::Copy;
    if(presetDropChoices(processes, copy).controlsAndState)
      return PresetDrop::ControlsAndState;
    return std::nullopt;
  }

  const auto choices = presetDropChoices(processes, copy);
  QMenu menu;
  auto add = [&](bool enabled, const QString& text, PresetDrop choice) {
    if(enabled)
      menu.addAction(text)->setData(int(choice));
  };
  add(choices.controls, QObject::tr("Cue of the controls"), PresetDrop::Controls);
  add(choices.controlsAndState, QObject::tr("Cue of the controls and the state"),
      PresetDrop::ControlsAndState);
  add(choices.copy, QObject::tr("Copy in a new box"), PresetDrop::Copy);
  if(menu.actions().empty())
    return std::nullopt;
  if(auto act = menu.exec(QCursor::pos()))
    return PresetDrop(act->data().toInt());
  return std::nullopt;
}
}
