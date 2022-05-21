#pragma once

#include <Library/LibraryInterface.hpp>
namespace Scenario
{
class SlotLibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("63767c12-c0cf-4ff2-9da6-d4378e1b2b74")

  bool onDrop(
      const QMimeData& mime,
      int row,
      int column,
      const QDir& parent) override;

  QSet<QString> acceptedMimeTypes() const noexcept override;

  QSet<QString> acceptedFiles() const noexcept override;
};

class ScenarioLibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("9e2d097d-9f0d-4f08-8cf8-bbf1975717db")

  bool onDrop(
      const QMimeData& mime,
      int row,
      int column,
      const QDir& parent) override;

  QSet<QString> acceptedMimeTypes() const noexcept override;

  QSet<QString> acceptedFiles() const noexcept override;
};
}
