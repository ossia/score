#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace Gfx::CSF
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("b5c5800f-2e84-4e29-9c7c-39577e6e6fa0")

  QSet<QString> acceptedFiles() const noexcept override;
};
}