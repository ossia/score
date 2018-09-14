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

QStringList LibraryInterface::acceptedFiles() const
{
  return {};
}

QStringList LibraryInterface::acceptedMimeTypes() const
{
  return {};
}

bool LibraryInterface::onDrop(
    FileSystemModel& model, const QMimeData& mime, int row, int column,
    const QModelIndex& parent)
{
  return false;
}
}
