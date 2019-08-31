#pragma once
#include <Process/ProcessContext.hpp>

#include <score/document/DocumentContext.hpp>

namespace Scenario
{
class ScenarioDocumentPresenter;
class BaseElementContext
{
public:
  const Process::Context& context;
  ScenarioDocumentPresenter& presenter;
};
}
