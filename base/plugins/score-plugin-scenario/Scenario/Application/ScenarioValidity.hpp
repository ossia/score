#pragma once
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Validity/ValidityChecker.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

namespace Scenario
{

class ScenarioValidityChecker final : public score::ValidityChecker
{
  SCORE_CONCRETE("f2dd8bec-a81b-4b3b-a57c-535001fde131")
public:
  virtual ~ScenarioValidityChecker();

  static void checkValidity(const Scenario::ProcessModel& scenar);

private:
  bool validate(const score::DocumentContext& ctx) override;
};
}
