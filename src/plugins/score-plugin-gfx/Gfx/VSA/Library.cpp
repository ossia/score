#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/VSA/Library.hpp>
#include <Gfx/VSA/Process.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/File.hpp>

#include <wobjectimpl.h>

namespace Gfx::VSA
{

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"vs", "vert"};
}

void LibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  // TODO relaunch whenever library path changes...
  const auto& key = Metadata<ConcreteKey_k, VSA::Model>::get();
  QModelIndex node = model.find(key);
  if(node == QModelIndex{})
    return;

  categories.init(Metadata<PrettyName_k, VSA::Model>::get().toStdString(), node, ctx);
}

void LibraryHandler::addPath(std::string_view path)
{
  score::PathInfo file{path};
  QFile f{file.absoluteFilePath.data()};
  if(!f.open(QIODevice::ReadOnly))
    return;
  auto sz = std::min((int64_t)4096, (int64_t)f.size());
  auto mapped = f.map(0, sz);
  if(!mapped)
    return;

  const auto haystack = std::span<const char>((const char*)mapped, sz);
  const auto needle = std::string_view("\"VERTEX_SHADER_ART\"");
  if(std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end())
     == haystack.end())
    return;

  Library::ProcessData pdata;
  pdata.prettyName
      = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());
  pdata.key = Metadata<ConcreteKey_k, VSA::Model>::get();
  pdata.customData = QString::fromUtf8(path.data(), path.size());
  categories.add(file, std::move(pdata));
}

QWidget*
LibraryHandler::previewWidget(const QString& path, QWidget* parent) const noexcept
{
  if(!qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW"))
    return new ShaderPreviewWidget{path, parent};
  else
    return nullptr;
}

QWidget* LibraryHandler::previewWidget(
    const Process::Preset& path, QWidget* parent) const noexcept
{
  if(!qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW"))
    return new ShaderPreviewWidget{path, parent};
  else
    return nullptr;
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"vs", "vert"};
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::VSA::Model>::get();
  p.creation.prettyName = filename.basename;
  p.creation.customData = filename.relative;

  vec.push_back(std::move(p));
}

}
