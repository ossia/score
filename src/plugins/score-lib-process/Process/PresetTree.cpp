#include "PresetTree.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <QObject>

#include <algorithm>

namespace Process
{

QStringList presetCategoryPath(const QString& category)
{
  QStringList path;
  for(const QString& part : category.split('/', Qt::SkipEmptyParts))
    if(auto p = part.trimmed(); !p.isEmpty())
      path.push_back(std::move(p));
  return path;
}

namespace
{
void flatten(const PresetTree::Node& n, std::vector<const Process::Preset*>& out)
{
  for(auto& e : n.entries)
  {
    if(e.preset)
      out.push_back(e.preset);
    else if(e.child >= 0)
      flatten(*n.children[std::size_t(e.child)], out);
  }
}
}

PresetTree::PresetTree(
    const Process::ProcessModel& proc, const std::vector<Process::Preset>& library,
    const std::vector<Process::Preset>& builtins)
{
  std::vector<const Process::Preset*> files;
  for(auto& preset : library)
    if(proc.presetMatches(preset))
      files.push_back(&preset);
  addLibrary(std::move(files));
  addBuiltins(builtins);
}

PresetTree::PresetTree(
    const Process::ProcessIdentifier& id, const std::vector<Process::Preset>& library,
    const std::vector<Process::Preset>& builtins)
{
  std::vector<const Process::Preset*> files;
  for(auto& preset : library)
    if(preset.key.key == id.key && (id.effect.isEmpty() || preset.key.effect == id.effect))
      files.push_back(&preset);
  addLibrary(std::move(files));
  addBuiltins(builtins);
}

void PresetTree::addBuiltins(const std::vector<Process::Preset>& builtins)
{
  if(builtins.empty())
    return;

  // A folder of its own, after every category: not merged with a library
  // category of the same name
  auto node = std::make_unique<Node>();
  node->name = builtinFolderName();
  root.entries.push_back({nullptr, int(root.children.size())});
  root.children.push_back(std::move(node));
  auto& builtin = *root.children.back();

  std::vector<std::pair<QStringList, const Process::Preset*>> own;
  own.reserve(builtins.size());
  for(auto& preset : builtins)
    own.emplace_back(presetCategoryPath(preset.category), &preset);
  std::stable_sort(own.begin(), own.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  });
  // A plug-in's presets keep the order it gives them, within their category
  for(auto& [path, preset] : own)
    folder(builtin, path).entries.push_back({preset, -1});
}

void PresetTree::addLibrary(std::vector<const Process::Preset*> presets)
{
  // Each category split once, not at each comparison
  std::vector<std::pair<QStringList, const Process::Preset*>> sorted;
  sorted.reserve(presets.size());
  for(auto* preset : presets)
    sorted.emplace_back(presetCategoryPath(preset->category), preset);
  std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
    if(lhs.first != rhs.first)
      return std::lexicographical_compare(
          lhs.first.begin(), lhs.first.end(), rhs.first.begin(), rhs.first.end());
    return lhs.second->name < rhs.second->name;
  });
  for(auto& [path, preset] : sorted)
    folder(root, path).entries.push_back({preset, -1});
}

PresetTree::Node& PresetTree::folder(Node& from, const QStringList& path)
{
  Node* current = &from;
  for(const QString& part : path)
  {
    auto it = current->childIndex.find(part);
    if(it == current->childIndex.end())
    {
      auto child = std::make_unique<Node>();
      child->name = part;
      const int index = int(current->children.size());
      current->children.push_back(std::move(child));
      current->entries.push_back({nullptr, index});
      it = current->childIndex.emplace(part, index).first;
    }
    current = current->children[std::size_t(it->second)].get();
  }
  return *current;
}

std::vector<const Process::Preset*> PresetTree::ordered() const
{
  std::vector<const Process::Preset*> out;
  flatten(root, out);
  return out;
}

QString PresetTree::builtinFolderName()
{
  return QObject::tr("Built-in");
}

namespace
{
constexpr auto presetCursorProperty = "score_presetCursor";
}

void rememberPreset(const Process::ProcessModel& proc, const Process::Preset& preset)
{
  const_cast<Process::ProcessModel&>(proc).setProperty(
      presetCursorProperty, QStringList{preset.category, preset.name, preset.path});
}

QStringList presetCursor(const Process::ProcessModel& proc)
{
  return proc.property(presetCursorProperty).toStringList();
}

void setPresetCursor(const Process::ProcessModel& proc, const QStringList& cursor)
{
  const_cast<Process::ProcessModel&>(proc).setProperty(presetCursorProperty, cursor);
}

bool isCurrentPreset(const Process::ProcessModel& proc, const Process::Preset& preset)
{
  const auto cursor = proc.property(presetCursorProperty).toStringList();
  // The file tells a library preset from a plug-in's own one of the same name
  return cursor.size() >= 2 && cursor[0] == preset.category && cursor[1] == preset.name
         && (cursor.size() < 3 || cursor[2] == preset.path);
}

bool loadPreset(
    const Process::ProcessModel& proc, const Process::Preset& preset,
    const score::DocumentContext& ctx)
{
  auto& factories = ctx.app.interfaces<LoadPresetCommandFactoryList>();
  auto cmd = factories.make(&LoadPresetCommandFactory::make, proc, preset, ctx);
  if(!cmd)
    return false;
  rememberPreset(proc, preset);
  CommandDispatcher<>{ctx.commandStack}.submit(cmd);
  ctx.app.applicationPlugin<Process::ApplicationPlugin>().currentPresetChanged(&proc);
  return true;
}
}
