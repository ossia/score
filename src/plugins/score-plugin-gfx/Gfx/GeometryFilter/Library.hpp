#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace Gfx::GeometryFilter
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("1261a519-06ce-4ae8-9584-d005ee5c9eb2")

  QSet<QString> acceptedFiles() const noexcept override;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;

  void addPath(std::string_view path) override;

  Library::Subcategories categories;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("e3a8ec68-262a-419a-b4bb-a7e0400f4c24")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropPath(
      std::vector<ProcessDrop>&, const score::FilePath& data,
      const score::DocumentContext& ctx) const noexcept override;
};
}
