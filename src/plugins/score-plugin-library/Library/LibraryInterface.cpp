#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <QDir>
#include <QSet>

namespace Library
{

LibraryInterface::~LibraryInterface() { }
LibraryInterfaceList::~LibraryInterfaceList() { }

void LibraryInterface::setup(
    ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
}

void LibraryInterface::addPath(std::string_view) { }

void LibraryInterface::removePath(std::string_view) { }

std::optional<ProcessEntry> LibraryInterface::scanPath(std::string_view)
{
  return std::nullopt;
}

void CategoryPaths::init(std::string processName, const score::ApplicationContext& ctx)
{
  auto p = std::make_shared<Paths>();
  QDir packages{ctx.settings<Library::Settings::Model>().getPackagesPath()};
  p->packagesRoot = packages.absolutePath().toStdString();
  p->presets = "Presets/" + std::move(processName);

  std::lock_guard lock{m_mutex};
  m_paths = std::move(p);
}

std::function<void()> LibraryInterface::asyncAddPath(std::string_view path)
{
  std::string p{path};
  return [this, p = std::move(p)]() { addPath(p); };
}

QSet<QString> LibraryInterface::acceptedFiles() const noexcept
{
  return {};
}

QSet<QString> LibraryInterface::acceptedMimeTypes() const noexcept
{
  return {};
}

QWidget*
LibraryInterface::previewWidget(const QString& path, QWidget* parent) const noexcept
{
  return nullptr;
}

QWidget* LibraryInterface::previewWidget(
    const QString& path, const QByteArray&, QWidget* parent) const noexcept
{
  // Whatever it can do with a path it has; nothing, for one it has not.
  return previewWidget(path, parent);
}

QWidget* LibraryInterface::previewWidget(
    const Process::Preset& path, QWidget* parent) const noexcept
{
  return nullptr;
}

bool LibraryInterface::onDrop(
    const QMimeData& mime, int row, int column, const QDir& parent)
{
  return false;
}

bool LibraryInterface::onDoubleClick(
    const QString& path, const score::DocumentContext& ctx)
{
  return false;
}

LibraryDocumentLoader::~LibraryDocumentLoader() { }

QSet<QString> LibraryDocumentLoader::acceptedFiles() const noexcept
{
  return {"score", "scorebin"};
}

bool LibraryDocumentLoader::onDoubleClick(
    const QString& path, const score::DocumentContext& ctx)
{
  ctx.app.docManager.loadFile(ctx.app, path);
  return true;
}

}
