#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace Gfx::CSF
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("b5c5800f-2e84-4e29-9c7c-39577e6e6fa0")

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
  SCORE_CONCRETE("b3adba36-29cc-45b4-bea3-5a2a89458a48")

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
