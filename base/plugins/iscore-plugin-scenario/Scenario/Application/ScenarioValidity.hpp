#pragma once
#include <Scenario/Process/ScenarioModel.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/command/Validity/ValidityChecker.hpp>
#include <iscore/document/DocumentContext.hpp>

namespace Scenario
{

class ScenarioValidityChecker final : public iscore::ValidityChecker
{
  ISCORE_CONCRETE_FACTORY("f2dd8bec-a81b-4b3b-a57c-535001fde131")
public:
  virtual ~ScenarioValidityChecker();

  static void checkValidity(const Scenario::ProcessModel& scenar);

private:
  bool validate(const iscore::DocumentContext& ctx) override;
};
}
