#include <Gfx/Filter/Library.hpp>
#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/File.hpp>
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

void LibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  // TODO relaunch whenever library path changes...
  const auto& key = Metadata<ConcreteKey_k, Filter::Model>::get();
  QModelIndex node = model.find(key);
  if(node == QModelIndex{})
    return;

  categories.init(node, ctx);
}

void LibraryHandler::addPath(std::string_view path)
{
  score::PathInfo file{path};

  Library::ProcessData pdata;
  pdata.prettyName
      = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());
  pdata.key = Metadata<ConcreteKey_k, Filter::Model>::get();
  pdata.customData = QString::fromUtf8(path.data(), path.size());
  categories.add(file, std::move(pdata));
}

QWidget*
LibraryHandler::previewWidget(const QString& path, QWidget* parent) const noexcept
{
  return new ShaderPreviewWidget{path, parent};
}

QWidget* LibraryHandler::previewWidget(
    const Process::Preset& path, QWidget* parent) const noexcept
{
  return new ShaderPreviewWidget{path, parent};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"frag", "glsl", "fs"};
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
  p.creation.prettyName = filename.basename;
  p.creation.customData = filename.relative;

  vec.push_back(std::move(p));
}
}
