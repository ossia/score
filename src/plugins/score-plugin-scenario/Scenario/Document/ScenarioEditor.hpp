#pragma once
#include <score/model/ObjectEditor.hpp>
namespace Scenario
{
class ScenarioEditor final : public score::ObjectEditor
{
  SCORE_CONCRETE("90265073-0aae-4628-834a-f44048664476")

  bool copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx) override;
  bool paste(QPoint pos, QObject* focusedObject, const QMimeData& mime, const score::DocumentContext& ctx) override;
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};

}
