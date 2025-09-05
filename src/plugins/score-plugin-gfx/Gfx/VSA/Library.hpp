#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace Gfx::VSA
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("e924dec9-21e7-4b4c-b81a-0813c22db4ea")

  QSet<QString> acceptedFiles() const noexcept override;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;

  void addPath(std::string_view path) override;
  QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept override;
  QWidget*
  previewWidget(const Process::Preset& path, QWidget* parent) const noexcept override;

  Library::Subcategories categories;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("78977726-e594-4d78-a9b2-09fc0f41afe3")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  void dropPath(
      std::vector<ProcessDrop>&, const score::FilePath& data,
      const score::DocumentContext& ctx) const noexcept override;

  void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const noexcept override;
};
}
