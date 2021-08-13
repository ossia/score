#include <Gfx/Filter/Library.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Gfx/Filter/PreviewWidget.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace Gfx::Filter
{

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"fs"};
}

void LibraryHandler::setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  // TODO relaunch whenever library path changes...
  const auto& key = Metadata<ConcreteKey_k, Filter::Model>::get();
  QModelIndex node = model.find(key);
  if (node == QModelIndex{})
    return;

  categories.parent
      = reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

  // We use the parent folder as category...
  categories.libraryFolder.setPath(
      ctx.settings<Library::Settings::Model>().getPath());
}

void LibraryHandler::addPath(std::string_view path)
{
  QFileInfo file{QString::fromUtf8(path.data(), path.length())};
  Library::ProcessData pdata;
  pdata.prettyName = file.baseName();
  pdata.key = Metadata<ConcreteKey_k, Filter::Model>::get();
  pdata.author = "ISF";
  pdata.customData = file.absoluteFilePath();
  categories.add(file, std::move(pdata));
}

QWidget* LibraryHandler::previewWidget(const QString& path, QWidget* parent)
    const noexcept
{
  return new ShaderPreviewWidget{path, parent};
}

QWidget* LibraryHandler::previewWidget(const Process::Preset& path, QWidget* parent)
    const noexcept
{
  return new ShaderPreviewWidget{path, parent};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"frag", "glsl", "fs"};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::dropData(
    const std::vector<DroppedFile>& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;
  {
    for (const auto& [filename, fragData] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.creation.customData = filename;

      vec.push_back(std::move(p));
    }
  }
  return vec;
}
}
