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
  SCORE_CONCRETE("c820b32d-8971-4d0e-9238-2b3d4e942d23")
public:
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;

private:
  QString getClapCategory(const QList<QString>& features) const;
};

}
