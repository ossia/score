#include "Library.hpp"

#include <Gfx/CSF/Metadata.hpp>
#include <Gfx/CSF/Process.hpp>

#include <QDir>
#include <QFileInfo>

namespace Gfx::CSF
{
QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"csf"};
}
}