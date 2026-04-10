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

std::function<void()> LibraryHandler::asyncAddPath(std::string_view path)
{
  score::PathInfo file{path};
  QFile f{file.absoluteFilePath.data()};

  if(!score::fileContains(f, "\"VERTEX_SHADER_ART\""))
    return {};

  Library::ProcessData pdata;
  pdata.prettyName
      = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());
  pdata.key = Metadata<ConcreteKey_k, VSA::Model>::get();
  pdata.customData = QString::fromUtf8(path.data(), path.size());

  return [this, p = std::string(path), pdata = std::move(pdata)]() mutable {
    score::PathInfo file{p};
    categories.add(file, std::move(pdata));
  };
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
