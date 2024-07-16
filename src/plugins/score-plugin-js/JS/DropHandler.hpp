#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <JS/JSProcessModel.hpp>

namespace JS
{

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("ad3a575a-f4a8-4a89-bb7e-bfd85f3430fe")

  QSet<QString> fileExtensions() const noexcept override { return {"qml"}; }

  void dropData(
      std::vector<ProcessDrop>& vec, const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    const auto& [filename, file] = data;
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, ProcessModel>::get();
    p.creation.prettyName = filename.basename;
    p.creation.customData = std::move(file);

    vec.push_back(std::move(p));
  }
};

}
