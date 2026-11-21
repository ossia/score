#pragma once

#include <Library/LibraryInterface.hpp>

#include <score_plugin_scenario_export.h>
namespace Scenario
{
class SlotLibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("63767c12-c0cf-4ff2-9da6-d4378e1b2b74")

  bool onDrop(const QMimeData& mime, int row, int column, const QDir& parent) override;

  QSet<QString> acceptedMimeTypes() const noexcept override;

  QSet<QString> acceptedFiles() const noexcept override;
};

//! .scenario files are the snippets: timeline content listed in the process
//! library, under its Snippets branch.
class ScenarioLibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("9e2d097d-9f0d-4f08-8cf8-bbf1975717db")

  bool onDrop(const QMimeData& mime, int row, int column, const QDir& parent) override;

  QSet<QString> acceptedMimeTypes() const noexcept override;

  QSet<QString> acceptedFiles() const noexcept override;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;
  std::optional<Library::ProcessEntry> scanPath(std::string_view path) override;
  //! The thumbnail it was saved with.
  QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept override;
};

//! The folders a snippet file is listed under: those below the snippets
//! folder it is in, or its own folder's name for one saved elsewhere.
SCORE_PLUGIN_SCENARIO_EXPORT
QStringList snippetCategories(std::string_view absoluteDir);

//! The library entry of a snippet file, none for another file.
SCORE_PLUGIN_SCENARIO_EXPORT
std::optional<Library::ProcessEntry> snippetEntry(std::string_view path);
}
