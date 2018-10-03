#pragma once
#include <Library/ProcessesItemModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#include <Media/ApplicationPlugin.hpp>

namespace Media::Faust
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("e274ee7b-9142-43a0-9d77-9286a63af4d9")

  QStringList acceptedFiles() const override
  {
    return {"*.dsp"};
  }
};
}
