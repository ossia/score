#include <Gfx/CSF/Library.hpp>
#include <Gfx/CSF/Process.hpp>
#include <Gfx/Filter/PreviewWidget.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/File.hpp>

#include <wobjectimpl.h>

namespace Gfx::CSF
{

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"cs", "comp", "csf"};
}

void LibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  categories.init(Metadata<PrettyName_k, CSF::Model>::get().toStdString(), ctx);
}

std::optional<Library::ProcessEntry> LibraryHandler::scanPath(std::string_view path)
{
  score::PathInfo file{path};
  QFile f{file.absoluteFilePath.data()};

  if(!score::fileContains(f, "\"COMPUTE_SHADER\""))
    return std::nullopt;

  Library::ProcessData pdata;
  pdata.prettyName
      = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());
  pdata.key = Metadata<ConcreteKey_k, CSF::Model>::get();
  pdata.customData = QString::fromUtf8(path.data(), path.size());

  return Library::ProcessEntry{pdata.key, categories(file), {std::move(pdata), {}}};
}

QWidget*
LibraryHandler::previewWidget(const QString& path, QWidget* parent) const noexcept
{
  return nullptr;
}

QWidget* LibraryHandler::previewWidget(
    const Process::Preset& path, QWidget* parent) const noexcept
{
  return nullptr;
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"cs", "comp", "csf"};
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::CSF::Model>::get();
  p.creation.prettyName = filename.basename;
  p.creation.customData = filename.relative;

  vec.push_back(std::move(p));
}

}
