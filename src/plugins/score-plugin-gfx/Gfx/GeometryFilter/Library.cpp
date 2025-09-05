#include <Gfx/GeometryFilter/Library.hpp>
#include <Gfx/GeometryFilter/PreviewWidget.hpp>
#include <Gfx/GeometryFilter/Process.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/File.hpp>

#include <QFile>
namespace Gfx::GeometryFilter
{

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"glsl"};
}

void LibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  // TODO relaunch whenever library path changes...
  const auto& key = Metadata<ConcreteKey_k, GeometryFilter::Model>::get();
  QModelIndex node = model.find(key);
  if(node == QModelIndex{})
    return;

  categories.init(
      Metadata<PrettyName_k, GeometryFilter::Model>::get().toStdString(), node, ctx);
}

void LibraryHandler::addPath(std::string_view path)
{
  score::PathInfo file{path};
  QFile f{path.data()};
  if(!f.open(QIODevice::ReadOnly))
    return;
  if(!f.read(512).contains("\"GEOMETRY_FILTER\""))
    return;

  Library::ProcessData pdata;

  pdata.prettyName
      = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());

  pdata.key = Metadata<ConcreteKey_k, GeometryFilter::Model>::get();
  pdata.customData = QString::fromUtf8(path.data(), path.size());
  categories.add(file, std::move(pdata));
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"glsl"};
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::GeometryFilter::Model>::get();
  p.creation.prettyName = filename.basename;
  p.creation.customData = filename.relative;

  vec.push_back(std::move(p));
}
}
