#include <Library/LibraryInterface.hpp>

namespace Library
{

LibraryInterface::~LibraryInterface()
{
}

void LibraryInterface::setup(
    ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
}

QSet<QString> LibraryInterface::acceptedFiles() const noexcept
{
  return {};
}

QSet<QString> LibraryInterface::acceptedMimeTypes() const noexcept
{
  return {};
}

bool LibraryInterface::onDrop(
    FileSystemModel& model, const QMimeData& mime, int row, int column,
    const QModelIndex& parent)
{
  return false;
}

bool LibraryInterface::onDoubleClick(const QString& path, const score::DocumentContext& ctx)
{
  return false;
}
}
