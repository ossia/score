#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <Clap/ApplicationPlugin.hpp>
#include <Clap/EffectModel.hpp>

#include <score/tools/Bind.hpp>

#include <verdigris>

namespace Clap
{

class LibraryHandler final : public QObject, public Library::LibraryInterface
{
  SCORE_CONCRETE("4a2b3c4d-5e6f-7a8b-9c0d-1e2f3a4b5c6d")
public:
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;

private:
  QString getClapCategory(const std::vector<QString>& features) const;
};

}