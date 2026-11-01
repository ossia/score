#include "PresetTree.hpp"

#include <Process/Process.hpp>

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
// By category, then by name: a folder's own presets sort before those of its
// subfolders, so they come first in it
bool presetOrder(const Process::Preset* lhs, const Process::Preset* rhs) noexcept
{
  const auto a = presetCategoryPath(lhs->category),
             b = presetCategoryPath(rhs->category);
  if(a != b)
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
  return lhs->name < rhs->name;
}

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

  std::vector<const Process::Preset*> own;
  for(auto& preset : builtins)
    own.push_back(&preset);
  std::stable_sort(own.begin(), own.end(), [](auto* lhs, auto* rhs) {
    return presetCategoryPath(lhs->category) < presetCategoryPath(rhs->category);
  });
  // A plug-in's presets keep the order it gives them, within their category
  for(auto* preset : own)
    folder(builtin, preset->category).entries.push_back({preset, -1});
}

void PresetTree::addLibrary(std::vector<const Process::Preset*> presets)
{
  std::sort(presets.begin(), presets.end(), presetOrder);
  for(auto* preset : presets)
    folder(root, preset->category).entries.push_back({preset, -1});
}

PresetTree::Node& PresetTree::folder(Node& from, const QString& category)
{
  Node* current = &from;
  for(const QString& part : presetCategoryPath(category))
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
      presetCursorProperty, QStringList{preset.category, preset.name});
}

bool isCurrentPreset(const Process::ProcessModel& proc, const Process::Preset& preset)
{
  const auto cursor = proc.property(presetCursorProperty).toStringList();
  return cursor.size() == 2 && cursor[0] == preset.category && cursor[1] == preset.name;
}
}
