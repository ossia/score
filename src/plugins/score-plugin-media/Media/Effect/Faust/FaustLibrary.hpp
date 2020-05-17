#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Media/Commands/InsertFaust.hpp>
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>

#include <QFileInfo>

namespace Media::Faust
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("e274ee7b-9142-43a0-9d77-9286a63af4d9")

  QSet<QString> acceptedFiles() const noexcept override { return {"dsp"}; }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("1e83a000-5aca-4427-8de5-1dc7a390e201")

  QSet<QString> fileExtensions() const noexcept override { return {"dsp"}; }

  std::vector<Process::ProcessDropHandler::ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

    for (auto&& [filename, file] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Media::Faust::FaustEffectModel>::get();
      // TODO use faust-provided name
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.creation.customData = std::move(file);

      vec.push_back(std::move(p));
    }

    return vec;
  }
};

}
